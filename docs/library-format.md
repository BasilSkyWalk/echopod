# Library database format (version 2)

The PC indexer (`tools/library_index.py`) writes the library database. The device reads it (`src/library`). Everything is sorted on the PC, so the device never sorts. It reads fixed-size records by index and strings by offset, a few hundred bytes at a time.

- Location on the card: `/.echo/library.db`
- Byte order: little-endian
- Indices: `u16` (at most 65,535 tracks, albums, groups and folders)
- `0xFFFF` means "none"

## Header (128 bytes)

| Offset | Type | Field |
|---|---|---|
| 0 | `char[4]` | magic `EMDB` |
| 4 | `u16` | version = 2 |
| 6 | `u16` | section count = 10 |
| 8 | 10 × 12 B | section directory, in the order below |

Each section directory entry is `u32 offset`, `u32 count`, `u16 record size`, `u16 reserved`.

| # | Section | Record size | Content |
|---|---|---|---|
| 0 | tracks | 24 | grouped by album in album order, then disc, track number and title |
| 1 | albums | 16 | sorted by title, then album artist |
| 2 | artists | 12 | track artists, sorted by name |
| 3 | album artists | 12 | sorted by name |
| 4 | genres | 12 | sorted by name |
| 5 | folders | 16 | depth-first; folder 0 is the music root (empty name) |
| 6 | title order | 2 | track indices sorted by title, then artist (All Songs) |
| 7 | playlists | 12 | M3U playlists, sorted by name |
| 8 | links | 2 | shared `u16` lists that groups, playlists and folders point into |
| 9 | strings | 1 | NUL-terminated UTF-8; offset 0 is the empty string; duplicates stored once |

## Records

| Record | Layout |
|---|---|
| **Track** | `u32 title`, `u32 path`, `u16 album`, `u16 artist`, `u16 genre`, `u16 folder`, `u32 duration_ms`, `u8 disc`, `u8 number`, `u16 reserved` |
| **Album** | `u32 title`, `u16 album_artist`, `u16 genre`, `u16 year`, `u16 first_track`, `u16 track_count`, `u16 cover` |
| **Group** (artist, album artist, genre, playlist) | `u32 name`, `u32 first_link`, `u16 link_count`, `u16 reserved` |
| **Folder** | `u32 name`, `u16 parent`, `u16 subfolder_count`, `u32 first_link`, `u16 track_count`, `u16 reserved` |

Field notes:
- `u32` string fields hold an offset into the strings section.
- Track `path` is relative to the music root and uses `/`. Every string is Unicode NFC (composed). FAT cards store names in NFC, but macOS lists them decomposed (NFD), so the indexer recomposes paths, playlist names, playlist entries and tags. The device then matches paths against the card's names without any Unicode tables. Track `artist` indexes the artists section, and `genre` indexes genres.
- An album's tracks are `tracks[first_track .. first_track + track_count)`. Album `cover` is reserved and always `0xFFFF`. Covers live in their own file, keyed by album key (see Covers file).
- A group's albums are `links[first_link .. first_link + link_count)`. Artists list every album containing at least one of their tracks. Artist and album-artist albums are ordered by year, then title; genre albums are ordered by title.
- A playlist's links are track indices in playlist order. Its name is the file name without `.m3u` or `.m3u8`.
- A folder's items are `links[first_link ..]`: first `subfolder_count` folder indices (sorted by name), then `track_count` track indices (sorted by disc, number, file name).

## Sorting

Sort keys are case-insensitive and accent-insensitive (Unicode NFKD with combining marks removed), and a leading "The " is ignored. So "The Beatles" sorts as "beatles" and "Björk" as "bjork".

## Files and tags

The indexer lists every format stock plays: `.flac`, `.ogg`, `.mp3`, `.m4a` (AAC or ALAC), `.wav`, `.wma`, `.ape`, `.dsf` and `.dff`, skipping hidden files and folders (a name starting with `.`). Each field takes the first non-empty value, trimmed:

| Field | Vorbis comment (FLAC, Ogg) | ID3 (MP3, WAV, DSF, DFF) | MP4 atom | APEv2 (any case) | ASF (WMA) |
|---|---|---|---|---|---|
| title | `title` | `TIT2` | `©nam` | `Title` | `Title` |
| artist | `artist` | `TPE1` | `©ART` | `Artist` | `Author` |
| album artist | `albumartist` | `TPE2` | `aART` | `Album Artist` | `WM/AlbumArtist` |
| album | `album` | `TALB` | `©alb` | `Album` | `WM/AlbumTitle` |
| genre | `genre` | `TCON` (ID3v1 numbers become names) | `©gen` | `Genre` | `WM/Genre` |
| date | `date` | `TDRC` (`TYER` in ID3v2.3) | `©day` | `Year` | `WM/Year` |
| disc | `discnumber` | `TPOS` | `disk` (first number) | `Disc` | `WM/PartOfSet` |
| track | `tracknumber` | `TRCK` | `trkn` (first number) | `Track` | `WM/TrackNumber` |

An MP3 without ID3v2 uses its ID3v1 tag. The duration is the container's length rounded to the nearest millisecond (FLAC and DSF sample counts, Ogg's last granule, an MP3's Xing/Info frame count or its bitrate, MP4's track duration, WAV data length, APE frame counts, ASF play duration minus preroll, DFF DSD data length). `tools/tag_fixture.py` writes one small file per format to `tests/fixtures/tags`, and both the Python indexer and the device reader (`src/library/track_tags`) are tested against them.

On the device, `track_tags_read` reads through a `ByteSource` with small bounded reads, keeps the first 255 bytes of each value (UTF-8, trimmed as Python's `strip()` does) and computes durations in double precision in the same order as mutagen, so the rounded milliseconds match. It also reports where the cover is: a front cover (picture type 3, or MP4's `covr`) wins over any other picture. For Ogg, the cover is the base64 `METADATA_BLOCK_PICTURE` text, and its offset counts bytes of the Ogg stream from the start of the comment packet's first page body, page headers excluded. Known differences from the PC indexer: values aren't recomposed to NFC, and an ID3v2.3 tag with tag-wide unsynchronisation is read without undoing it.

## Building on the device

`library/library_builder` builds the same database on the device from tracks given in scan order (the PC indexer's sorted path order), inside one scratch block it's handed. Strings are interned once in a hash table (a slot per 32 bytes of scratch, at most 75 % full); paths are kept as a folder node plus the file name; track records grow from the bottom of the block and strings from the top. `library_builder_finish` reproduces each step of `build_library` with stable merge sorts on 16-bit indices (first spelling wins per sort key, most common genre with ties to the first seen, earliest non-zero year, album order by first sight then title and album artist, preorder folders without recursion, chronological and alphabetical group links, All Songs by title then artist), then writes the file in two passes: string offsets are assigned in the PC's order first, then the sections stream out through a 512-byte buffer, and the `EMDB` magic goes in last so an interrupted build reads as no library. It runs out of memory rather than truncating. Two fixtures check it byte for byte: `small_library_tracks.db` and `builder_random/` (400 seeded tracks with spelling variants, accents, shared titles, missing years and nested folders, from `tools/builder_fixture.py`). A path is never shared with another string in the string table; the PC would share one only if a title or name were the same text as a whole relative path.

`library/library_scan` walks the card for it through a `ScanFileSystem` (list a folder, open a file): each folder is read whole, names starting with `.` are dropped, the rest sorted by their UTF-8 bytes (the PC's part-wise path order), and the walk goes depth first without recursion, with one folder open at a time. Files with a stock extension get `track_tags_read` and `library/track_fields` (the fallbacks below), then go to the builder. A file whose tags can't be read is skipped and counted. An eighth of the scratch block (16–96 KB) holds the folder listings; the builder gets the rest. Its test scans `tests/fixtures/tags` through the simulator's file HAL and must match `tests/fixtures/tags_library.db`, which `tags_library_up_to_date` re-indexes with the PC tool. On 2026-10-18 it also matched the PC indexer byte for byte on a 50-track sample folder.

## Fallbacks for missing tags

| Missing | Used instead |
|---|---|
| title | file name without its number prefix |
| artist | album artist, then "Unknown Artist" |
| album artist | artist |
| album | folder name |
| genre | "Unknown Genre" |
| disc and number | `<disc>-<track> - ` or `<track> - ` file-name prefixes, otherwise disc 1, track 0 |
| year | first four digits of `date`, otherwise 0 |

## Playlists

The indexer reads every `.m3u` and `.m3u8` file under the music root, skipping hidden folders. A file is decoded as UTF-8 (with or without a BOM), or as Windows-1252 if that fails. Blank lines and `#` lines are ignored, and so are URLs.

Each entry is matched to a track, ignoring case and treating `\` as `/`:

1. relative to the playlist's folder
2. relative to the music root
3. otherwise by the longest matching run of trailing path parts, so absolute Windows paths such as MusicBee's `D:\Music\Artist\Album\01 - Song.flac` still match. A tie counts as not found.

Entries that match nothing are left out, and the indexer reports how many each playlist lost.

On the device, the scanner notes each `.m3u`/`.m3u8` path during the walk (the PC's order) and reads them after the tracks, once `library_builder_prepare` has fixed the track order and built the path indexes: an exact table keyed by the casefolded path (Python's `str.casefold`, from a second generated table; the last of equal keys wins, as in a Python dict) and chains by casefolded file name for the suffix match. `library/playlist_reader` checks the whole file for strict UTF-8 (a BOM is skipped) or decodes Windows-1252 with U+FFFD for its five undefined bytes, splits lines like `str.splitlines` (U+0085, U+2028 and U+2029 only in UTF-8) and trims them; each entry is resolved at once, so entries are never stored. Candidates go through `posixpath.normpath` rules, including its two leading slashes. Playlists are sorted by sort key and then name, and their names share the string table with everything else. `scan_music/` (nested folders, a hidden folder, an AppleDouble file and three playlists in UTF-8 with a BOM, Windows-1252 and plain UTF-8) and `builder_random/with_playlists` (30 random playlists, 379 entries) check it byte for byte.

## Album keys and favorites

Favorites outlive a re-index, so they don't store album indices. An album's key is the FNV-1a hash of its album artist, a `0x1F` separator and its title (`library_album_key`). `/.echo/favorites.bin` holds the magic `EMFV`, `u16` version 1, a `u16` count, the `u32` keys and an FNV-1a checksum. At most 256 albums can be favorites. An album that leaves the library just stops showing up.

## Covers file

`tools/cover_export.py` writes `/.echo/covers.bin`. It's separate from the database, keyed by album key, so either file can be rebuilt on its own.

| Offset | Type | Field |
|---|---|---|
| 0 | `char[4]` | magic `EMCV` |
| 4 | `u16` | version = 1 |
| 6 | `u16` | cover count |
| 8 | `u16` | full size = 112 |
| 10 | `u16` | half size = 56 |
| 12 | `u32` | reserved (0) |
| 16 | count × 8 B | index: `u32 album key`, `u32 record offset`, sorted by key |

Each record is the full cover (112 × 112 pixels) followed by the half cover (56 × 56), as little-endian RGB565 rows, top to bottom. That's 31,360 bytes per album, so about 6.8 MB for 217 albums.

The exporter takes, for each album:
1. the first embedded front cover (picture type 3) in any of its tracks
2. otherwise the first embedded picture of any type
3. otherwise `cover`, `folder`, `front` or `albumart` (`.jpg`, `.jpeg` or `.png`, any case) in a folder holding its tracks

The image is cropped to a centered square and resized with Lanczos filtering, separately for each size. Albums without art are left out and listed when the export finishes; the device draws a plain placeholder for them.

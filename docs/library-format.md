# Library database format (version 3)

The PC indexer (`tools/library_index.py`) writes the library database. The device reads it (`src/library`). Everything is sorted on the PC, so the device never sorts. It reads fixed-size records by index and strings by offset, a few hundred bytes at a time.

- Location on the card: `/.echo/library.db`
- Byte order: little-endian
- Indices: `u16` (at most 65,535 tracks, albums, groups and folders)
- `0xFFFF` means "none"

## Header (128 bytes)

| Offset | Type | Field |
|---|---|---|
| 0 | `char[4]` | magic `EMDB` |
| 4 | `u16` | version = 3 (the player rebuilds a library of any other version) |
| 6 | `u16` | section count = 10 |
| 8 | 10 × 12 B | section directory, in the order below |

Each section directory entry is `u32 offset`, `u32 count`, `u16 record size`, `u16 music count`. The music count is set for tracks, albums, artists, album artists, genres and title order (0 elsewhere); see Spoken word below.

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
- Track `path` is relative to the music root and uses `/`. A track on the player's internal storage (Settings → Internal storage) has a path under `:internal/`, a top-level folder no FAT or exFAT card can hold (`:` isn't allowed in names): `:internal/Artist/Album/01.flac` is `Artist/Album/01.flac` on internal storage. The folder is sorted among the root's names by its bytes like any other, and its folder record is named `:internal` (the player shows it as "Internal storage"). A library without internal storage has no such paths. Every string is Unicode NFC (composed). FAT cards store names in NFC, but macOS lists them decomposed (NFD), so the indexer recomposes paths, playlist names, playlist entries and tags. The device then matches paths against the card's names without any Unicode tables. Track `artist` indexes the artists section, and `genre` indexes genres.
- An album's tracks are `tracks[first_track .. first_track + track_count)`. Album `cover` is reserved and always `0xFFFF`. Covers live in their own file, keyed by album key (see Covers file).
- A group's albums are `links[first_link .. first_link + link_count)`. Artists list every album containing at least one of their tracks. Artist and album-artist albums are ordered by year, then title; genre albums are ordered by title. Within each section the groups' links are consecutive and in group order (group `n + 1` starts where group `n` ends). Cover Flow relies on this for album artists to read its order straight from `links`; a file that breaks it gets Cover Flow in album order instead.
- A playlist's links are track indices in playlist order. Its name is the file name without `.m3u` or `.m3u8`.
- A folder's items are `links[first_link ..]`: first `subfolder_count` folder indices (sorted by name), then `track_count` track indices (sorted by disc, number, file name).

## Spoken word

A track is spoken word when its first folder is `Audiobooks`, `Audiobook`, `Podcasts` or `Podcast` (ASCII case-insensitive; `library/spoken_path`, `spoken_of` in the indexer). Tracks on internal storage have `:internal` as their first folder, so they are always music. Spoken word is kept out of the Music lists and lives on the Audiobooks and Podcasts shelves, which browse its folders.

Every sort and grouping key of a spoken track starts with a spoken flag that sorts after music, so spoken word never shares an album, artist, album artist or genre with music: an author with music too appears twice, once among the music groups and once among the spoken ones. Each of those sections and the title order therefore hold their music entries first, then the spoken ones, and the header's music count says where the music ends. Tracks follow album order, so spoken tracks come last too. The Music lists (Cover Flow, Albums, Artists, Album Artists, Genres, Songs, shuffle) only read the first `music count` entries; folders and playlists show everything. Covers are built for every album, spoken ones included.

## Sorting

Sort keys are case-insensitive and accent-insensitive (Unicode NFKD with combining marks removed), and a leading "The " is ignored. So "The Beatles" sorts as "beatles" and "Björk" as "bjork".

## Files and tags

The indexer lists every format stock plays, `.flac`, `.ogg`, `.mp3`, `.m4a` (AAC or ALAC), `.wav`, `.wma`, `.ape`, `.dsf` and `.dff`, and also `.opus`, skipping hidden files and folders (a name starting with `.`). Each field takes the first non-empty value, trimmed:

| Field | Vorbis comment (FLAC, Ogg, Opus) | ID3 (MP3, WAV, DSF, DFF) | MP4 atom | APEv2 (any case) | ASF (WMA) |
|---|---|---|---|---|---|
| title | `title` | `TIT2` | `©nam` | `Title` | `Title` |
| artist | `artist` | `TPE1` | `©ART` | `Artist` | `Author` |
| album artist | `albumartist` | `TPE2` | `aART` | `Album Artist` | `WM/AlbumArtist` |
| album | `album` | `TALB` | `©alb` | `Album` | `WM/AlbumTitle` |
| genre | `genre` | `TCON` (ID3v1 numbers become names) | `©gen` | `Genre` | `WM/Genre` |
| date | `date` | `TDRC` (`TYER` in ID3v2.3) | `©day` | `Year` | `WM/Year` |
| disc | `discnumber` | `TPOS` | `disk` (first number) | `Disc` | `WM/PartOfSet` |
| track | `tracknumber` | `TRCK` | `trkn` (first number) | `Track` | `WM/TrackNumber` |
| album version | `albumversion` | `TXXX` described `ALBUMVERSION` (any case) | `----` named `ALBUMVERSION` (any case, any mean) | `AlbumVersion` | not read |

**Album versions.** When a track has both an album tag and an album version (the `ALBUMVERSION` tag Navidrome uses to tell editions apart; Picard users usually set it from `%_releasecomment%` with a script), the album title becomes `Album (Version)`, for example `Abbey Road (2019 Remix)`. The version is part of the title everywhere (lists, Now Playing, Cover Flow, sorting) and of the album key, so two editions of one album are two albums, each with its own cover and favorite. Albums without a version keep their key, so favorites and stored covers of those survive. A version without an album tag is ignored (the folder name stays the album). Navidrome also reads `MUSICBRAINZ_ALBUMCOMMENT` as the version; EchoPod doesn't.

An MP3 without ID3v2 uses its ID3v1 tag. The duration is the container's length rounded to the nearest millisecond (FLAC and DSF sample counts, Ogg's last granule (for Opus minus the header's pre-skip, at 48 kHz), an MP3's Xing/Info frame count or its bitrate, MP4's track duration, WAV data length, APE frame counts, ASF play duration minus preroll, DFF DSD data length). `tools/tag_fixture.py` writes one small file per format to `tests/fixtures/tags`, and both the Python indexer and the device reader (`src/library/track_tags`) are tested against them.

On the device, `track_tags_read` reads through a `ByteSource` with small bounded reads, keeps the first 255 bytes of each value (UTF-8, trimmed as Python's `strip()` does; on both sides, invisible marks and control codes are dropped, bytes 0x80–0x9F read as Windows-1252, and the typographic spaces U+2000–U+200A, U+2028, U+2029, U+202F and U+205F become plain spaces, which the fonts can draw) and computes durations in double precision in the same order as mutagen, so the rounded milliseconds match. It also reports where the cover is: a front cover (picture type 3, or MP4's `covr`) wins over any other picture. For Ogg, the cover is the base64 `METADATA_BLOCK_PICTURE` text, and its offset counts bytes of the Ogg stream from the start of the comment packet's first page body, page headers excluded; the cover pass decodes that text as it reads it (see `docs/architecture.md`, Covers on the device). Known differences from the PC indexer: values aren't recomposed to NFC, and an ID3v2.3 tag with tag-wide unsynchronisation is read without undoing it.

## Building on the device

`library/library_builder` builds a database that means the same as the PC indexer's, from tracks given in scan order (the PC indexer's sorted path order), with a block of RAM and a scratch file on the card (`/.echo/build.tmp`, 24 MB, reserved with the other player files). It handles up to 65,535 tracks; 20,000 are tested inside 360 KB of RAM. RAM only ever holds what one step needs: buffers, small caches and a few 16-bit columns.
- During the scan each track becomes a 48-byte row on the card (text offsets and lengths, folder, year, disc, number, duration) and its texts go to a heap on the card (a text equal to the previous track's same field is stored once). Folders become 8-byte rows; a hash of (parent, name) in RAM finds a folder seen before.
- Before ranking, tracks marked as having no album-artist tag (a spare row byte) get their shared album artist: one record per such track (folder, track, artist text, album and artist sort keys) is sorted by folder, album key, artist key and track; one pass picks each group's winner, a second writes (track, artist text) pairs, which are sorted by track and merged back into the rows in one sequential rewrite. Nothing runs when no track is marked.
- Each field is then ranked: one record per run of equal text (its sort key, its bytes and its first track) goes through a stable external merge sort on the card (sorted chunks in RAM, then up to 32-way merges), giving a dense key rank per track, a spelling rank for genres, and each group's first spelling. Folder names are ranked by key, then bytes.
- Every view is built from integer tuples with the same sort, in the PC's order: albums (grouped by album artist and album; first sight, earliest non-zero year, most common genre spelling with ties to the first seen; members by disc, number and title), album order (title, then album artist, then first sight), the final track order, artist and album-artist links by year and title, genre links by title, preorder folders (sorted rank paths) with their links, and All Songs by title then artist. Playlists resolve through RAM tables of 16-bit positions keyed by path hashes, confirmed against the paths rebuilt from the card.
- `library_builder_finish` writes the file in two passes over the card data: records with string offsets counted in emission order, then the strings, through a 512-byte buffer; the `EMDB` magic goes in last so an interrupted build reads as no library. Titles and paths are written per track and names per group, without the PC's global de-duplication, so the bytes differ from the PC's file while every record, link and string means the same. It reports running out of memory or scratch room rather than truncating.
- Tests decode both files and compare them field by field (`tests/library/library_compare.c`): the fixtures `small_library_tracks.db` and `builder_random/`, and seeded random libraries of 1 to 20,000 tracks (`tests/library/random_library.c`) against the previous in-RAM builder kept as a test-only reference (`tests/library/reference/`).

`library/library_scan` walks the card for it through a `ScanFileSystem` (list a folder, open a file): each folder is read whole, names starting with `.` are dropped, the rest sorted by their UTF-8 bytes (the PC's part-wise path order), and the walk goes depth first without recursion, with one folder open at a time. Files with a stock extension get `track_tags_read` and `library/track_fields` (the fallbacks below), then go to the builder. A file whose tags can't be read is skipped and counted. An eighth of the scratch block (16–96 KB) holds the folder listings unless a separate listing buffer is given; the builder gets the rest and the scan job's scratch file. Its test scans `tests/fixtures/tags` through the simulator's file HAL and must mean the same as `tests/fixtures/tags_library.db`, which `tags_library_up_to_date` re-indexes with the PC tool. On 2026-10-18 it also matched the PC indexer byte for byte on a 50-track sample folder.

## Fallbacks for missing tags

| Missing | Used instead |
|---|---|
| title | file name without its number prefix |
| artist | album artist, then "Unknown Artist" |
| album artist | artist, then shared per album (below) |
| album | folder name (not for the card root or the `:internal` folder: "Unknown Album") |
| genre | "Unknown Genre" |
| disc and number | `<disc>-<track> - ` or `<track> - ` file-name prefixes, otherwise disc 1, track 0 |
| year | first four digits of `date`, otherwise 0 |

**Albums without an album-artist tag.** Tracks whose album artist came from the artist (no album-artist tag) and that share a folder and an album title (by sort key) are one album: they all get the album artist that most of them have as artist, ties going to the one whose sort key comes first, spelled as the earliest such track spells it. So an album with a featured artist on some tracks ("The Weeknd & Justice" among "The Weeknd") stays one album instead of splitting. Tracks with an album-artist tag keep it, and the same album split over folders (`CD1`, `CD2`) still joins when their artists agree. The PC indexer does this in `guess_album_artists` before building; the device builder does it as a pass over the track rows before ranking (below).

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

## Bookmarks file

`/.echo/bookmarks.bin` (4 KB, reserved by the player) remembers where each audiobook and podcast episode stopped (`playback/bookmarks`). Like favorites, it is keyed by path hashes, so it outlives a re-index.

| Offset | Type | Field |
|---|---|---|
| 0 | `char[4]` | magic `EMBK` |
| 4 | `u16` | version = 1 |
| 6 | `u16` | count (at most 128) |
| 8 | `u32` | reserved (0) |
| 12 | count × 16 B | bookmarks, most recent first |
| 12 + 16 × count | `u32` | FNV-1a checksum of everything before it |

Each bookmark is `u32 key`, `u32 track key`, `u32 position_ms`, `u16 chapter`, `u8 flags` (bit 0: finished), `u8 reserved`. The key is the FNV-1a hash of the book's folder path relative to the music root (`Audiobooks/The Long Road`), or of the track's own path for a single-file book or a podcast episode; the track key hashes the track's path. `chapter` is the track's place among its folder's tracks, checked against the track key before it's used. A zero-filled or damaged file reads as no bookmarks.

## Update record

`/.echo/update.bin` (4 KB, reserved by the player) keeps what the firmware installer last did, so Diagnostics → Last update can show it after the player has been switched off (`app/update_record`, written by `board/update.c` at the end of a USB session that found a `HIFIECnn.IMG`). Every value is little-endian.

| Offset | Type | Field |
|---|---|---|
| 0 | `char[4]` | magic `EUPD` |
| 4 | `u8` | result: 0 none, 1 installed, 2 failed, 3 refused, 4 already installed |
| 5 | `u8` | detail: the last progress mark shown (5 % steps) for installed and failed, the refusal code (`E` number) for refused |
| 6 | `u8` | 1 when the internal storage answered CMD13 after the install |
| 7 | `u8` | reserved (0) |
| 8 | `u32` | the eMMC status word that CMD13 returned (card state in bits 12:9, ready-for-data in bit 8) |
| 12 | `u32` | checksum: the bitwise complement of a 31-multiplier sum over the first 12 bytes |

A zero-filled or damaged file reads as no record.

## Listen history file

`/.echo/history.bin` (1 MiB, reserved by the player) logs the songs the player played. The player turns it into `/.scrobbler.log` itself whenever it connects to a computer, and `tools/echo_history.py` can do the same on a PC (see `docs/user-guide.md`, Listen history, and below). It's a ring of 2,048 slots of 512 bytes; the player works out the slot count from the file's size, so a smaller or larger file works too. A record with sequence number `n` always sits in slot `(n − 1) mod slots`, so when the ring is full the newest record overwrites the oldest. Every value is little-endian.

| Offset | Type | Field |
|---|---|---|
| 0 | `char[4]` | magic `EHST` |
| 4 | `u32` | sequence number, from 1, one more for every record |
| 8 | `u32` | session, one more for every start of the player that writes a record |
| 12 | `u8` | kind: 1 = a play, 2 = connected to a computer |
| 13 | `u8` | track number (0 when unknown) |
| 14 | `u16` | reserved (0) |
| 16 | `u32` | start: milliseconds since the player started, when the play began |
| 20 | `u32` | end: milliseconds since the player started, when the play ended |
| 24 | `u32` | Unix time at the end, or 0 when the player doesn't know the time |
| 28 | `u32` | milliseconds actually listened (pauses and skipped-over parts don't count) |
| 32 | `u32` | the track's length in milliseconds (0 when unknown) |
| 36 | `u32` | position in the track where the play began, in milliseconds |
| 40 | `u32` | position in the track where the play ended, in milliseconds |
| 44 | 464 B | artist, album and title, each UTF-8 and NUL-terminated, then zeros |
| 508 | `u32` | FNV-1a checksum of bytes 0–507 |

The names are copied from the library when the play starts, not stored as indexes, so a rescan doesn't change them: the artist (the library already falls back to the album artist, then "Unknown Artist"), the album title and the track title. The artist and the album take at most 159 bytes each and the title gets the rest, cut at a whole UTF-8 character. A "connected to a computer" record (kind 2) has its start and end at the moment the USB session began and no names.

A slot is a record when its magic, its checksum and its place (`(sequence − 1) mod slots`) all agree; anything else (zeros, a record torn by a power cut, data left on the card from before the file was made) is skipped. To find where to write, the player reads slot 0 and then bisects for the first slot that doesn't hold the next sequence number. If slot 0 isn't a record, it checks the last slot, so a torn write over slot 0 after a wrap continues from the right number; otherwise it starts at 1. The session is one more than the newest record's.

Dating the records. The Unix-time field holds the UTC time the record was written (the player's local clock minus its time zone setting) and is 0 while the clock or the zone is unknown; the tool dates a session from its newest record with a time when there is one (earlier records of the same session, written before the clock was set, follow from it); otherwise it counts back from the moment it runs (`--now`), taken as the time of the newest record. That's exact for the session the player is in when it's connected (the "connected to a computer" record is written at that moment) and for every play in that session. An earlier session is placed so that its last record ends when the next session started: the time the player was off between them is unknown and counted as none, so those plays can be dated later than they really happened (by however long the player was off).

## Scrobbler log and export marker

`/.scrobbler.log` at the card's root is the AudioScrobbler 1.1 log Rockbox writes and Last.fm/ListenBrainz uploaders read: UTF-8 with LF line ends, three header lines (`#AUDIOSCROBBLER/1.1`, `#TZ/UTC`, `#CLIENT/EchoPod <version>`), then one line per play: artist, album, title, track number (empty when 0), length in seconds, `L` (listened) or `S` (skipped), the Unix time (UTC) the play started, and an empty MusicBrainz ID, separated by tabs and ending with a tab and a newline. Tabs and line breaks inside names become spaces. A play is `L` when the track is over 30 s and it was listened to for half its length or 4 minutes (parts joined across a power-off or USB session count together, see above); otherwise its last part is `S`. The player writes both, like Rockbox; `echo_history.py` writes only `L` unless asked (`--include-skipped`). The player adds lines at the end of an existing log (after a newline if the last line has none), writes the header into a missing or empty one, and keeps the file exactly as long as its text.

`/.echo/history-exported.txt` is the sequence number of the newest record already exported, as ten digits and a newline (`0000000042`), shared by the player and `echo_history.py`; both export only records with a higher number, so a log the uploader deleted comes back with just the new plays. Either reads the leading number (older versions of the tool wrote it without the zeros) and treats a number above the ring's newest record (a new history file) as 0. The player only writes it after the lines are on the card; when it can't write them all (the card is full) it marks the last record it did write. Plays in sessions it can't date (no timed record in that session or any later one) are left unmarked for a later connection or the PC tool.

## Music folders file

`/.echo/folders.bin` (reserved at 4 KB by `card_files`; 140 bytes used, little-endian) lists the top-level folders Settings → Music folders leaves out of the library. A missing, zeroed or damaged file leaves nothing out.

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `EMMF` |
| 4 | 1 | version, 1 |
| 5 | 1 | count, at most 32 |
| 6 | 2 | reserved, 0 |
| 8 | 128 | 32 folder keys (`u32`), the first `count` used, the rest 0 |
| 136 | 4 | FNV-1a of bytes 0–135 |

A folder key is the FNV-1a hash of the folder's library path: its name as the file system lists it (UTF-8, case kept) for a folder in the card's root, `:internal/` followed by the name for a folder in the internal storage's root. Keys name folders rather than positions, so new folders are included and a folder that is gone keeps its choice. Both the player's scan and fingerprint and `library_index.py --folders` / echo-sync skip a top-level folder (never a file, never a deeper folder) whose key is listed; `library_index.encode_excluded_folders` writes the file. `tests/fixtures/scan_music_folders.bin` leaves `Artist B` and `Lists` out of `scan_music`, and `scan_music_folders.db` is the PC indexer's library for it (`scan_music_folders_up_to_date`); `test_library_scan` scans the same folder with the same file and must mean the same.

## Clock record

`/.echo/clock.bin` (20 bytes, little-endian) is written by echo-sync as the last step of a sync, so the player can set its clock chip from the computer's when the USB session ends (see `docs/architecture.md`, Clock). The player never writes it.

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `ECLK` |
| 4 | 1 | version, 1 |
| 5 | 1 | flags, 0 |
| 6 | 2 | reserved, 0 |
| 8 | 4 | the computer's time as Unix seconds (UTC) |
| 12 | 2 | the computer's UTC offset in minutes, signed (`time.localtime().tm_gmtoff`) |
| 14 | 2 | reserved, 0 |
| 16 | 4 | FNV-1a of bytes 0–15 |

The player accepts a record whose magic, version and checksum agree and whose time falls in 2024–2099, and only when its time differs from the record it saw when it started: it sets the chip to the time plus the offset (the chip keeps local time), stores the offset as its time zone setting (rounded to 15 minutes), and ignores the same record from then on.

## Covers file

The player builds `/.echo/covers.bin` after every rescan (see `docs/architecture.md`, Covers on the device); `tools/cover_export.py` writes the same format on a PC. It's separate from the database, keyed by album key, so either file can be rebuilt on its own.

| Offset | Type | Field |
|---|---|---|
| 0 | `char[4]` | magic `EMCV` |
| 4 | `u16` | version = 1 |
| 6 | `u16` | cover count |
| 8 | `u16` | full size = 112 |
| 10 | `u16` | half size = 56 |
| 12 | `u32` | stamp table offset, or 0 for none |
| 16 | count × 8 B | index: `u32 album key`, `u32 record offset`, sorted by key |

Each record is the full cover (112 × 112 pixels) followed by the half cover (56 × 56), as little-endian RGB565 rows, top to bottom. That's 31,360 bytes per album, so about 6.8 MB for 217 albums. Readers follow the record offsets, so records may start after a larger index: the player reserves index room for every album that fits the file before it knows how many have art.

The player also keeps a stamp per cover so a rescan can reuse it (see `docs/architecture.md`, Covers on the device). For a file of `capacity` bytes it has room for `slots = (capacity − 16) / (8 + 4 + 31,360)` covers: the index area holds `slots` entries, the stamp table follows at `16 + slots × 8` with a `u32` stamp for each index entry in the same order, and record `n` starts at `16 + slots × 12 + n × 31,360`. Readers that don't use stamps ignore the table. A stamp of 0 means unknown: the player decodes that album again. `docs/architecture.md` (Covers on the device) defines the stamp.

echo-sync (`encode_stamped`) writes this layout for the card's `covers.bin` size, with the stamps the player would compute: for embedded art from the track's size and the picture's length (the same bytes mutagen returns; for Ogg, the image inside the decoded `METADATA_BLOCK_PICTURE` block), for a cover file from its size and the card's modification stamp (`card_stamp`, as for the music fingerprint). It writes 0 for a cover file when the card isn't FAT or exFAT, since the time can't be matched. `tools/cover_export.py` on its own (`--output`) still writes the packed layout, with 0 at offset 12 and records right after the index; the player reads such a file normally and rebuilds every cover on its next rescan.

The exporter takes, for each album:
1. the embedded picture of the first of its first four tracks (in track order) that has one: the track's first front cover (picture type 3), otherwise its first picture of any type. For Ogg Vorbis and Opus files these are the `METADATA_BLOCK_PICTURE` comments; a value that isn't valid base64 or a valid picture block is skipped
2. otherwise `cover`, `folder`, `front` or `albumart` (`.jpg`, `.jpeg` or `.png`, any case) in a folder holding its tracks

The image is cropped to a centered square and resized with Lanczos filtering, separately for each size. Albums without art are left out and listed when the export finishes; the device draws a plain placeholder for them.

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
- Track `path` is relative to the music root and uses `/`. Track `artist` indexes the artists section, and `genre` indexes genres.
- An album's tracks are `tracks[first_track .. first_track + track_count)`. Album `cover` is reserved and always `0xFFFF`. Covers live in their own file, keyed by album key (see Covers file).
- A group's albums are `links[first_link .. first_link + link_count)`. Artists list every album containing at least one of their tracks. Artist and album-artist albums are ordered by year, then title; genre albums are ordered by title.
- A playlist's links are track indices in playlist order. Its name is the file name without `.m3u` or `.m3u8`.
- A folder's items are `links[first_link ..]`: first `subfolder_count` folder indices (sorted by name), then `track_count` track indices (sorted by disc, number, file name).

## Sorting

Sort keys are case-insensitive and accent-insensitive (Unicode NFKD with combining marks removed), and a leading "The " is ignored. So "The Beatles" sorts as "beatles" and "Björk" as "bjork".

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

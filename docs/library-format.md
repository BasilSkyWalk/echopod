# Library database format (version 1)

The PC indexer (`tools/library_index.py`) writes the library database. The device reads it (`src/library`). Everything is sorted on the PC, so the device never sorts. It reads fixed-size records by index and strings by offset, a few hundred bytes at a time.

- Location on the card: `/.echo/library.db`
- Byte order: little-endian
- Indices: `u16` (at most 65,535 tracks, albums, groups and folders)
- `0xFFFF` means "none"

## Header (128 bytes)

| Offset | Type | Field |
|---|---|---|
| 0 | `char[4]` | magic `EMDB` |
| 4 | `u16` | version = 1 |
| 6 | `u16` | section count = 9 |
| 8 | 9 × 12 B | section directory, in the order below |
| 116 | 12 B | zero padding |

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
| 7 | links | 2 | shared `u16` lists that groups and folders point into |
| 8 | strings | 1 | NUL-terminated UTF-8; offset 0 is the empty string; duplicates stored once |

## Records

| Record | Layout |
|---|---|
| **Track** | `u32 title`, `u32 path`, `u16 album`, `u16 artist`, `u16 genre`, `u16 folder`, `u32 duration_ms`, `u8 disc`, `u8 number`, `u16 reserved` |
| **Album** | `u32 title`, `u16 album_artist`, `u16 genre`, `u16 year`, `u16 first_track`, `u16 track_count`, `u16 cover` |
| **Group** (artist, album artist, genre) | `u32 name`, `u32 first_link`, `u16 album_count`, `u16 reserved` |
| **Folder** | `u32 name`, `u16 parent`, `u16 subfolder_count`, `u32 first_link`, `u16 track_count`, `u16 reserved` |

Field notes:
- `u32` string fields hold an offset into the strings section.
- Track `path` is relative to the music root and uses `/`. Track `artist` indexes the artists section, and `genre` indexes genres.
- An album's tracks are `tracks[first_track .. first_track + track_count)`. Album `cover` is `0xFFFF` until the cover exporter fills it (Phase 1f).
- A group's albums are `links[first_link .. first_link + album_count)`. Artists list every album containing at least one of their tracks. Artist and album-artist albums are ordered by year, then title; genre albums are ordered by title.
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

# EchoPod roadmap

Status: `device` (works on the player) · `sim` (works in the simulator only) · `in progress` · `todo`

## Where it stands

Version 0.6.0 makes Bluetooth connect within seconds and adds Random album and album grouping across featured artists; 0.5.0 added Bluetooth headphones, a charge screen and a steadier battery percent; 0.4.0 showed Chinese, Japanese and Korean names; 0.3.0 made USB DAC mode play on more computers and phones, accepted FiiO's 4.0.0 firmware for the way back, added gapless MP3 and kept covers across rescans; 0.2.0 added audiobooks and podcasts, USB DAC mode and echo-sync (`docs/release-notes.md`). Everything marked `device` below works on the one Echo Mini 8 GB it has been tested on. `docs/known-issues.md` lists what's missing or unproven.

## Features

| Feature | Status | Notes |
|---|---|---|
| FLAC, MP3 and WAV playback | device | Our own FLAC decoder on the second core with the hardware LPC accelerator; libmad for MP3; 24-bit output at 44.1–192 kHz |
| Gapless playback and fades | device | DAC fades around play, pause, seek, skip and rate changes; MP3s trimmed by their LAME tag's encoder delay and padding (tested on a computer, not yet heard on the player) |
| Resume after power-off | device | The exact queue and position come back |
| Library built on the player | device | At first start, after music changes over USB, and from Settings → Rescan library; tested up to 6,558 tracks on the player and 20,000 on a computer; 1,563 tracks and 144 albums in about 1 min 45 s |
| Album covers built on the player | device | Embedded art or `cover`/`folder`/`front`/`albumart` JPEG or PNG; room for 2,673 albums; a rescan decodes only new or changed art (simulator and tests; not yet timed on the player) |
| Browse: Artists, Album Artists, Albums, Genres, Folders, All Songs, Playlists, Favorites | device | Fast scroll with letter jumps in A–Z lists |
| Cover Flow | device | Every music album, by album artist; a Random album tile first |
| Now Playing | device | Cover, progress, seek, modes, volume |
| Queue, shuffle, repeat, Up Next | device | Shuffle Songs or Albums over lists of any size; Play Next and Add to Up Next |
| M3U and M3U8 playlists | device | Including MusicBee's Windows paths |
| Album favorites | device | Hold Play on an album |
| Audiobooks and podcasts | device | `Audiobooks` and `Podcasts` folders on Home, kept out of Music; a bookmark per book and episode, in-order play, hold vol± jumps 30 s / 15 s (shelves checked on the player, bookmarks and jumps in the simulator) |
| Library and covers built on a computer | device | echo-sync, one file for Windows, macOS and Linux, writes `library.db`, `covers.bin` and the scan fingerprint so the player doesn't rescan; 1,563 tracks in 2 min on Windows (macOS and Linux untested on a card) |
| Screen-off controls | device | Pause, volume and track skip with the screen off |
| Settings | device | Shuffle, repeat, sleep timer, screen timeout, auto off, brightness, gain, output, start screen, USB mode |
| Battery and power | device | Battery level, charging, charge screen, low-battery shutdown, auto off |
| Bluetooth headphones | device | Pair, remember up to six, auto-reconnect, switch and disconnect; headphone controls; tested with OnePlus Buds 3 |
| USB mass storage | device | Internal storage and the SD card, read/write, at high speed; SD card reads at about 3.9 MB/s |
| USB DAC mode | device | Settings → USB mode → DAC: a USB Audio Class 1.0 sound card, 16 or 24-bit at 44.1 or 48 kHz, with its own remembered volume; played on the player from Windows 11 (all four formats) and an Android phone |
| Updates and the way back to FiiO | device | EchoPod installs its own updates and FiiO's 8 GB firmware from a file copy |
| Configurable start screen | sim | Works in the simulator; not yet checked on the player |

## Next

| Item | Status |
|---|---|
| An assigned USB product ID from pid.codes, replacing the shared test ID | todo |
| Refuse to start on an almost empty battery | todo |
| Check the 4.4 mm balanced output on hardware | done (tester report) |
| Check the post-0.1.0 fixes and audiobook bookmarks on hardware | in progress |
| Faster cover decoding on the player (about 0.5 s per album) | todo |
| Browse while covers are built | todo |
| echo-sync on a Mac and on Linux with a real card | todo |
| Firmware images in GitHub releases | todo |
| Playback speed for audiobooks and podcasts | todo |
| Installing from a Mac through FiiO's updater | todo |
| Testing on more players | todo |

## Later

| Item | Status |
|---|---|
| Parametric EQ | todo |
| ReplayGain | todo |
| Lyrics (`.lrc`) | todo |
| ALAC playback | done |
| AAC playback | done |
| HE-AAC playback | todo |
| Opus playback | done |
| APE playback | todo |

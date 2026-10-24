# EchoPod roadmap

Status: `device` (works on the player) · `sim` (works in the simulator only) · `in progress` · `todo`

## Where it stands

Version 0.1.0 is the first public build. Everything marked `device` below works on the one Echo Mini 8 GB it has been tested on. `docs/known-issues.md` lists what's missing or unproven.

## Features

| Feature | Status | Notes |
|---|---|---|
| FLAC, MP3 and WAV playback | device | Our own FLAC decoder on the second core with the hardware LPC accelerator; libmad for MP3; 24-bit output at 44.1–192 kHz |
| Gapless playback and fades | device | DAC fades around play, pause, seek, skip and rate changes |
| Resume after power-off | device | The exact queue and position come back |
| Library built on the player | device | At first start, after music changes over USB, and from Settings → Rescan library; tested up to 6,558 tracks on the player and 20,000 on a computer |
| Album covers built on the player | device | Embedded art or `cover`/`folder`/`front`/`albumart` JPEG or PNG; room for 2,674 albums |
| Browse: Artists, Album Artists, Albums, Genres, Folders, All Songs, Playlists, Favorites | device | Fast scroll with letter jumps in A–Z lists |
| Cover Flow | device | Every album, by album artist |
| Now Playing | device | Cover, progress, seek, modes, volume |
| Queue, shuffle, repeat, Up Next | device | Shuffle Songs or Albums over lists of any size; Play Next and Add to Up Next |
| M3U and M3U8 playlists | device | Including MusicBee's Windows paths |
| Album favorites | device | Hold Play on an album |
| Screen-off controls | device | Pause, volume and track skip with the screen off |
| Settings | device | Shuffle, repeat, sleep timer, screen timeout, auto off, brightness, gain, output, start screen |
| Battery and power | device | Battery level, charging, low-battery shutdown, auto off |
| USB mass storage | device | Internal storage and the SD card, read/write, at high speed |
| Updates and the way back to FiiO | device | EchoPod installs its own updates and FiiO's 8 GB firmware from a file copy |
| Configurable start screen | sim | Works in the simulator; not yet checked on the player |

## Next

| Item | Status |
|---|---|
| An assigned USB product ID from pid.codes, replacing the shared test ID | todo |
| Refuse to start on an almost empty battery | todo |
| Check the 4.4 mm balanced output on hardware | todo |
| Installing from a Mac through FiiO's updater | todo |
| Testing on more players | todo |

## Later

| Item | Status |
|---|---|
| Bluetooth audio | in progress (the module powers up, scans and pairs; no audio yet) |
| Parametric EQ | todo |
| ReplayGain | todo |
| Lyrics (`.lrc`) | todo |
| USB DAC mode | todo |
| ALAC and APE playback | todo |

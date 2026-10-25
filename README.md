# EchoPod

Replacement firmware for the Snowsky (FiiO) Echo Mini 8 GB music player. It has an iPod-style Cover Flow browser, a music library built on the player, album favorites, M3U playlists and an Up Next queue. It isn't made or supported by FiiO.

- **Installing and using it:** `docs/user-guide.md`.
- **Read first:** `docs/known-issues.md`. EchoPod is early, and has been tested on one player.
- **What's new:** `docs/release-notes.md`.

The hardware knowledge it relies on comes largely from the [ReChord](https://github.com/brnomt/ReChord) project's research (`docs/hardware.md`, `CREDITS.md`).

## Build and run the simulator (macOS)

```sh
brew install cmake sdl2 clang-format arm-none-eabi-gcc arm-none-eabi-binutils
python3 -m venv .venv && .venv/bin/pip install -r tools/requirements.txt
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/platform/sim/echo_sim
```

## Build on Windows

Install [MSYS2](https://www.msys2.org) and, in its CLANG64 shell, the packages `mingw-w64-clang-x86_64-{clang,compiler-rt,cmake,ninja,SDL2}` and `mingw-w64-clang-x86_64-arm-none-eabi-{gcc,binutils,newlib}`, plus Python 3 with `pip install -r tools/requirements.txt`. With `C:\msys64\clang64\bin` first on the PATH (and not `C:\msys64\usr\bin`, whose git ignores this repo's line endings):

```sh
CMAKE_GENERATOR=Ninja CC=clang cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

## Build the firmware

```sh
cmake --preset release
cmake --build --preset release
```

This writes `build/release/HIFIEC90.IMG`. The `device` preset builds the same image with the developer Diagnostics rows. `docs/device-install.md` covers installing builds during development.

## Use your music library in the simulator

```sh
mkdir -p build/system
./build/dev/platform/sim/echo_sim --card /path/to/music --system-dir build/system
```

The simulator behaves like the player: on the first start it creates `library.db`, `covers.bin` and the save files in the system folder, scans the music folder (every format stock lists, plus `.m3u`/`.m3u8` playlists) and finds the album covers. Settings → Rescan library scans again. The music folder is only ever read.

The PC tools still work and write the same formats (`tools/library_index.py`, `tools/cover_export.py`), but the player doesn't need them. To skip the scan on the player, **echo-sync** builds the library, the covers and the music fingerprint (`scan.bin`) on a computer, straight into the card's `/.echo` folder, so the player's check after USB finds nothing to rescan (`docs/user-guide.md`, Build the library on a computer). Each release carries it as one file for Windows, macOS (Apple silicon) and Linux, with nothing to install, built by `.github/workflows/echo-sync.yml` with PyInstaller; from the source it's `tools/echo_sync.py E:`. To build the file yourself: `python -m pip install -r tools/requirements.txt -r tools/requirements-build.txt`, then `python -m PyInstaller --onefile --name echo-sync --paths tools tools/echo_sync.py`.

## Testing on a player

1. Install the firmware (`docs/user-guide.md`, or `docs/device-install.md` for development builds).
2. Put music on a microSD card (FAT32 or exFAT), in any folder layout, and insert it.
3. Switch on. The player creates `/.echo` on the card, scans it ("Scanning library", then "Finding covers") and shows "Library: N tracks, M covers".
4. After copying music over USB, eject both drives: the player rescans by itself. Settings → Rescan library does it on demand.

Nothing else on the card needs a computer. Covers come from art embedded in each album's first track (FLAC, MP3/WAV/DSF/DFF ID3, MP4, APE, WMA) or a `cover`/`folder`/`front`/`albumart` JPEG or PNG next to the tracks.

### Simulator keys

| Device button | Keyboard |
|---|---|
| Volume up (moves menus down) | Down arrow |
| Volume down (moves menus up) | Up arrow |
| Play | Return or Space |
| Menu | M or Tab |
| Back | Backspace or Escape |
| Power | P |

### Simulator options

| Option | Effect |
|---|---|
| `--scale N` | Window scale (default 3) |
| `--frames N` | Quit after N frames |
| `--headless` | No window |
| `--screenshot FILE.bmp` | Save the last frame on exit |
| `--card DIR` | Use DIR as the card root (read only) |
| `--system-dir DIR` | Read and write `/.echo/` (library database, resume, settings, favorites) in DIR |
| `--battery N` | Report N % battery in the header (default 80) |
| `--charging` | Report the battery as charging |
| `--keys LIST` | Play a key script, e.g. `down,down,play,back:long` (`up`, `down`, `play`, `menu`, `back`, `power`, each optionally `:long`) |

Headless runs use a virtual clock, so scripted screenshots are deterministic.

### Navigation

| Button | In lists | On Now Playing | While seeking |
|---|---|---|---|
| Volume up / down | Next / previous item (wraps on a single press, not while held) | Tap for volume; hold Vol+ for the next track, Vol− for the previous one | Forward / back 5 s per tap, 15 s steps while held |
| Play | Select (a song plays the list from that song); hold on an album to add or remove it from Favorites | Play / pause; hold to seek | Jump to the chosen time |
| Menu | | | |
| Back | Back; hold to go home | Back; hold to go home | Cancel |
| Power | Screen off or on; hold 2 s to turn off | Same | Same |

The screen turns off after the screen timeout or a tap of Power. With the screen off, ▶‖ pauses and resumes, a tap of vol± changes the volume and holding vol± skips tracks, all without waking the screen; Power turns it back on.

Home also lists Audiobooks and Podcasts when the card has an `Audiobooks` or `Podcasts` folder: every book and episode remembers where you stopped, plays in order, and hold vol± jumps 30 s forward or 15 s back. Books and episodes stay out of the Music lists (`docs/user-guide.md`).

Settings: Shuffle (Off / Songs / Albums), Repeat (Off / One / All), Sleep timer, Screen timeout, Brightness, Gain, Output and Start screen. They're saved in `/.echo/settings.bin`, and favorites in `/.echo/favorites.bin`. Without `--system-dir` the simulator can't save, and says so. Playback resumes, paused, where it left off.

Music → Cover Flow shows your albums as covers, by album artist. Vol± move, Play opens the album, holding Play adds or removes it from Favorites, and Back leaves. Music → Playlists lists the M3U playlists the indexer found. Music → Favorites lists your favorite albums.

## Layout

See `docs/architecture.md`.

## License

GPL-3.0-or-later. See `LICENSE`. The bundled Inter font is under the SIL Open Font License 1.1 (`assets/fonts/inter/OFL.txt`). The simulator's decoders (dr_flac, dr_mp3, dr_wav) are public domain or MIT-0 (`third_party/dr_libs/LICENSE`). The player's MP3 decoder is libmad, GPL-2.0-or-later (`third_party/libmad/COPYING`).

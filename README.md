# Echo Mini firmware

A custom firmware app layer for the Snowsky Echo Mini digital audio player. It has an iPod-style Cover Flow browser, a library database, album favorites and M3U playlists. Development happens in a macOS simulator; the device port builds on [ReChord](https://github.com/brnomt/ReChord).

Status: early development. Nothing here can be flashed yet.

## Build and run the simulator (macOS)

```sh
brew install cmake sdl2 clang-format arm-none-eabi-gcc arm-none-eabi-binutils
python3 -m venv .venv && .venv/bin/pip install -r tools/requirements.txt
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/platform/sim/echo_sim
```

### Use your music library

```sh
.venv/bin/python tools/library_index.py --music /path/to/music --output build/library/library.db
.venv/bin/python tools/cover_export.py --music /path/to/music --output build/library/covers.bin
./build/dev/platform/sim/echo_sim --card /path/to/music --system-dir build/library
```

The indexer and the cover exporter only read the music folder. The exporter uses embedded front covers, or a `cover`/`folder` image next to the tracks. On the device, the database goes in `/.echo/library.db` on the card, and the covers in `/.echo/covers.bin`. The format is described in `docs/library-format.md`.

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
| Power | Lock or unlock; hold to turn off | Same | Same |

The screen turns off after the screen timeout. Any key wakes it and does nothing else. While locked, only Power works.

Settings: Shuffle (Off / Songs / Albums), Repeat (Off / One / All), Sleep timer, Screen timeout, Brightness, Gain, Output and Start screen. They're saved in `/.echo/settings.bin`, and favorites in `/.echo/favorites.bin`. Without `--system-dir` the simulator can't save, and says so. Playback resumes, paused, where it left off.

Music → Cover Flow shows your albums as covers, by album artist. Vol± move, Play opens the album, holding Play adds or removes it from Favorites, and Back leaves. Music → Playlists lists the M3U playlists the indexer found. Music → Favorites lists your favorite albums.

## Layout

See `docs/architecture.md`.

## License

GPL-3.0-or-later. See `LICENSE`. The bundled Inter font is under the SIL Open Font License 1.1 (`assets/fonts/inter/OFL.txt`). The simulator's decoders (dr_flac, dr_mp3, dr_wav) are public domain or MIT-0 (`third_party/dr_libs/LICENSE`).

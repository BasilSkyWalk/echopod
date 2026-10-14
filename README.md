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
./build/dev/platform/sim/echo_sim --card /path/to/music --system-dir build/library
```

The indexer only reads the music folder. On the device, the database goes in `/.echo/library.db` on the card. The format is described in `docs/library-format.md`.

### Simulator keys

| Device button | Keyboard |
|---|---|
| Volume up | Up arrow |
| Volume down | Down arrow |
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
| `--system-dir DIR` | Read `/.echo/` (the library database) from DIR |
| `--keys LIST` | Play a key script, e.g. `down,down,play,back:long` (`up`, `down`, `play`, `menu`, `back`, `power`, each optionally `:long`) |

Headless runs use a virtual clock, so scripted screenshots are deterministic.

### Navigation

| Button | In lists | On Now Playing |
|---|---|---|
| Volume up / down | Previous / next item (wraps on a single press, not while held) | Volume |
| Play | Select (a song plays the list from that song) | Play / pause |
| Menu | | Next track; hold for previous |
| Back | Back; hold to go home | Back; hold to go home |

Settings has Shuffle (Off / Songs / Albums) and Repeat (Off / One / All). Playback resumes, paused, where it left off.

## Layout

See `docs/architecture.md`.

## License

GPL-3.0-or-later. See `LICENSE`. The bundled Inter font is under the SIL Open Font License 1.1 (`assets/fonts/inter/OFL.txt`). The simulator's decoders (dr_flac, dr_mp3, dr_wav) are public domain or MIT-0 (`third_party/dr_libs/LICENSE`).

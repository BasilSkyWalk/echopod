# Echo Mini firmware

A custom firmware app layer for the Snowsky Echo Mini digital audio player. It has an iPod-style Cover Flow browser, a library database, album favorites and M3U playlists. Development happens in a macOS simulator; the device port builds on [ReChord](https://github.com/brnomt/ReChord).

Status: early development. Nothing here can be flashed yet.

## Build and run the simulator (macOS)

```sh
brew install cmake sdl2 clang-format arm-none-eabi-gcc arm-none-eabi-binutils
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/platform/sim/echo_sim
```

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

## Layout

See `docs/architecture.md`.

## License

GPL-3.0-or-later. See `LICENSE`.

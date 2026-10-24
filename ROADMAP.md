# Echo Mini firmware — roadmap

Status: `todo` · `in progress` · `sim` (works in the simulator) · `device` (works on hardware) · `blocked`

## Phases

| Phase | Work | Estimate | Device | Status |
|---|---|---|---|---|
| 0 | Recon of ReChord, architecture proposal | — | no | done |
| 0.5 | Repo setup, toolchains, cross-build check on macOS | 0.5–1 d | no | done, except the ReChord cross-build (blocked, see NOTES) |
| 1a | SDL simulator, HAL, frame-time counter, memory-budget enforcement | 2–3 d | no | done |
| 1b | UI toolkit: screen stack, lists, marquee, bitmap fonts from TTF | 3–5 d | no | done |
| 1c | PC indexer, DB format, browse screens | 4–6 d | no | done |
| 1d | Simulator playback: queue, shuffle/repeat, gapless, resume | 3–4 d | no | done |
| 1e | Now Playing, settings, favorites, M3U, configurable home | 4–6 d | no | done |
| 1f | Cover Flow and cover export script | 4–6 d | no | done |
| 2 | Recovery dry run on our unit (maskrom detect only, no writes), stock backup | 1–2 d | yes | done 2026-10-14: maskrom detection, plus writing the stock 3.7.0 image back with `wl`, both pass on macOS; stock boots after |
| 3a | First device image: our UI on our own display, key, SD/eMMC and power-off drivers, plus USB mass storage and an updater | 1–2 wk | yes | done 2026-10-14 on the test unit: boot, keys, power-off, internal storage, SD card (exFAT, read/write over USB), USB mass storage, our updater, the library and covers from the card, a firmware tag |
| 3b | UX polish on the device: smooth Cover Flow (faster pushes, partial redraws), screen-off controls, Smart Play, start-up and shut-down screens, control feedback, a cleaner Now Playing, slide transitions, an Up Next queue | 1–2 wk | yes | done 2026-10-20 on the device: Cover Flow, screen-off controls, Smart Play, start-up and goodbye screens, auto Now Playing, control feedback, cleaner Now Playing, slide transitions, menu restyle, Up Next (Play Next / Add to Up Next, saved across power-off), fast scrolling with letter jumps and a charging bolt |
| 3 | Device gaps: PLL, IRQs/SysTick, DMA audio, FAT writes, battery, backlight PWM, key timing, sleep | 3–8 wk | yes | in progress: PLL, DMA ring to I2S, FLAC decoding on core 1, key interrupt, FAT writes and file creation, battery and low-battery shutdown, backlight PWM and idle power all work on the device; library and covers are built on the player (2026-10-19); the audio phase is done on the device (2026-10-20): 24-bit output, jack detect, fades, WAV, MP3, 192 kHz. Left: a watchdog, the boot cutoff, a 4.4 mm jack check and 176.4 kHz |
| 3c | USB speed: multi-block card writes, the music check after USB, high-speed USB with a full-speed fallback, Windows build and test setup | 1–2 d | yes | done 2026-10-21 on Windows 11: writes 5,178 KB/s to internal storage and 3,780 KB/s to the SD card (from 180/210 KB/s); unplugging no longer sits on a stale screen. Left: macOS at high speed, a byte-exact read-back at high speed, and the parked overlap of USB with card programming |
| 4 | Our app on the device; memory and Cover Flow tuning | 1–2 wk | yes | todo |
| R | Release readiness: our own image header (no RECHO bytes), maskrom from Windows, what the stock 3.7.0–3.9.0 updaters accept, a way back to stock, a release build, an install guide and packaging | 1–3 wk | yes | in progress (approved 2026-10-22): steps 1–2 done (generated header, maskrom from Windows); step 3 on hardware 2026-10-24: stock 3.7.0 installs our image by copy and restart from Windows on the test unit. Next: the lost USB delete and the SD dirty flag, a way back to stock, then the release build as EchoPod |

## Features

### MVP

| Feature | Status | Notes |
|---|---|---|
| FLAC / MP3 / WAV playback | device | dr_libs in the simulator. On the device: our own FLAC decoder (`src/codec`, bit-exact on the test files) decoding on core 1 with a DMA ring engine, at the right rate, 24-bit output. WAV (PCM 8–32-bit, core 0) and MP3 (libmad on core 1, loaded from the image) on the device, picked by file content; a refused track says why. 176.4 and 192 kHz via the FLAC accelerator on core 1 (on hardware 2026-10-20) |
| ALAC / APE | todo | Only leaked SDK binaries exist; deferred |
| Gapless | device | Token-based hand-over through the audio HAL, with DAC fades around play, pause, seek, skip and rate changes |
| Resume after power-off | device | `/.echo/resume.bin`, overwritten in place (written on hardware 2026-10-18); the player now creates the file itself on a new card |
| Library DB from SD scan | device | Built on the player: on a new card at first start, from Settings → Rescan library, and after USB changes the card; every stock format and playlists, byte for byte like the PC (`tools/library_index.py`). The player creates its own `/.echo` files (FAT16/FAT32/exFAT, fsck-checked) and builds `covers.bin` after each scan. On hardware 2026-10-19 (`6c56fe3`): scan and covers took about 2 minutes for 1,414 tracks, 129 covers matching the PC export |
| Browse: Artist / Album Artist / Album / Genre / Folder / All Songs / Playlists / Favorites | device | Selecting a track plays the list from it. On the device; holding the rocker speeds up, and A–Z lists show the letter and jump by letter |
| Now Playing: art, info, progress, seek | device | Cover, info, progress, times, state, modes, volume and seek |
| Queue, shuffle, repeat | device | Shuffle Songs/Albums, Repeat One/All, seeded. Up Next: songs added by hand play before the queue continues |
| Volume and button mapping, lock/hold | device | Rocker = volume (hold on Now Playing = previous/next track). Power short press = lock/unlock, long press = off; wake on any key |
| Settings: brightness, screen timeout, sleep timer, gain, output | in progress | Saved in `/.echo/settings.bin` (written in place on the device). Brightness drives stock's 10 kHz backlight PWM on the device; gain works; Output Auto follows the jack (stock's 3.5 or 4.4 mm curve); unplugging pauses |
| Battery, charging, power management | in progress | Battery percent, charging state (green fill and a bolt beside the battery), screen timeout, power-off, charger detection (no USB mode without a computer) and stock's clock policy, WFI while dark and auto off (5 min) on the device; the Power-tap power-offs are fixed (2026-10-20: a key stamp after the loop's time read as a long press). Low-battery shutdown works; jack detect works on 3.5 mm; no watchdog or boot cutoff yet |
| USB mass storage | device | Internal storage and SD card, both read/write, at high speed (480 Mb/s) with a once-per-power-on full-speed fallback. Multi-block card writes through a 32 KB buffer. Measured on Windows 11 (2026-10-21): 5,178 KB/s to internal storage, 3,780 KB/s to the SD card. macOS at high speed not yet checked |

### Ours

| Feature | Status | Notes |
|---|---|---|
| Cover Flow album browser | device | Perspective covers from a 69 KB cache, by album artist. Covers are built on the player after each rescan (JPEG baseline and progressive, PNG) |
| Album favorites | device | Hold Play on an album; Music → Favorites |
| Configurable home screen | sim | Start screen setting: Main menu, Now Playing, Albums, Artists, Favorites, Playlists |
| M3U playlists | device | Read and resolved by the player when it builds the library, like the PC indexer, including MusicBee's Windows paths |

### Later

| Feature | Status | Notes |
|---|---|---|
| Parametric EQ | todo | ReChord's DSP chain has an EQ module (host-tested) |
| ReplayGain | todo | ReChord's player supports track/album (host-tested) |
| Lyrics (.lrc) | todo | |
| USB DAC mode | todo | |
| Bluetooth | in progress | External module with its own stack: GPIO2.7 power, UART1 115,200 ASCII commands, audio likely analog from the DAC (facts 13). Step 1 done on hardware (`d0df057`): power, ready check and scan list work. Step 2 done on hardware (`f1893b8`): pairing works. Next: a Bluetooth settings screen with reconnect, then audio. Not a priority for now (2026-10-20) |

## Platform

| Piece | Status | Notes |
|---|---|---|
| Simulator (SDL2, 320×170 RGB565 at 3×, keyboard keys) | sim | Frame stats in the window title; `--headless --frames N --screenshot` for automated checks |
| HAL: display, input, clock, files, audio, power | sim | Display power and brightness, audio gain and output, battery and power-off added in 1e |
| Key gestures: press, long press, repeat | sim | Tested in `core` |
| Strip renderer (no full framebuffer) | sim | 320×34 strips |
| Quality gates in ctest: no comments, clang-format, Cortex-M3 RAM/heap audit | done | The audit also reports flash per module |
| Windows host build (MSYS2 CLANG64) | done | Builds and runs every test; the device image builds too with the RECHO reference in place (NOTES, Build facts) |
| Fonts: Inter 12/16/SemiBold 16, 4-bit anti-aliased, UTF-8 | sim | Generated by `tools/font_build.py`, checked by `fonts_up_to_date` |
| UI toolkit: screen stack, navigation intents, list view (scroll, wrap, scrollbar, details, marquee, hold actions), menus, choice lists, messages, toasts, header with status icons | sim | |
| Menu tree: Home → Music (library views, Playlists, Favorites; Cover Flow placeholder), Now Playing, Settings, Diagnostics (key test, fonts, long names, frame-time overlay) | sim | |
| Scripted simulator runs (`--keys`, virtual clock) | done | For deterministic screenshots |

## Tools

| Tool | Status |
|---|---|
| Font builder (Python) | done |
| Library indexer (Python) | done |
| Cover exporter (raw RGB565 thumbnails + index) | done |

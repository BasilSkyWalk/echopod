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
| 3b | UX polish on the device: smooth Cover Flow (faster pushes, partial redraws), screen-off controls, Smart Play, start-up and shut-down screens, control feedback, a cleaner Now Playing, slide transitions, an Up Next queue | 1–2 wk | yes | in progress: faster Cover Flow (`-O2`, frame split), screen-off controls, Smart Play, start-up and goodbye screens, auto Now Playing (on wake), control feedback done; cleaner Now Playing and slide transitions done (sim; Now Playing confirmed on device); menu restyle done (sim); Up Next still to do |
| 3 | Device gaps: PLL, IRQs/SysTick, DMA audio, FAT writes, battery, backlight PWM, key timing, sleep | 3–8 wk | yes | in progress: PLL (48 MHz boot clock found, 96/200 MHz switch works, starts at 96 MHz), DAC bring-up and test tones, DMA ring to I2S work on hardware; FLAC plays at the right rate since the I2S format fix, but core 0 can't decode 96 kHz / 24-bit in real time at any clock; next is decoding on core 1 with the FLAC accelerator (see NOTES, Next session); FAT writes, battery, backlight PWM still todo |
| 4 | Our app on the device; memory and Cover Flow tuning | 1–2 wk | yes | todo |

## Features

### MVP

| Feature | Status | Notes |
|---|---|---|
| FLAC / MP3 / WAV playback | in progress | dr_libs in the simulator. On the device: our own FLAC decoder (`src/codec`, bit-exact on the test files) and a DMA ring engine; plays at the right rate on the device but too slowly for 96 kHz on core 0, so decoding moves to core 1 (NOTES, Next session). MP3/WAV not on the device |
| ALAC / APE | todo | Only leaked SDK binaries exist; deferred |
| Gapless | sim | Token-based hand-over through the audio HAL |
| Resume after power-off | sim | `/.echo/resume.bin`; needs FAT writes on the device |
| Library DB from SD scan | sim | Built on the PC (`tools/library_index.py`); an on-device rescan comes later |
| Browse: Artist / Album Artist / Album / Genre / Folder / All Songs / Playlists / Favorites | sim | Selecting a track plays the list from it |
| Now Playing: art, info, progress, seek | in progress | Info, progress, times, state, modes, volume and seek done; the cover isn't shown on Now Playing yet |
| Queue, shuffle, repeat | sim | Shuffle Songs/Albums, Repeat One/All, seeded |
| Volume and button mapping, lock/hold | sim | Rocker = volume (hold on Now Playing = previous/next track). Power short press = lock/unlock, long press = off; wake on any key |
| Settings: brightness, screen timeout, sleep timer, gain, output | sim | Saved in `/.echo/settings.bin`. On the device, brightness needs PWM, gain the CS43131 and output jack routing |
| Battery, charging, power management | in progress | Header battery icon, screen timeout, lock and power-off work in the sim. No battery driver yet |
| USB mass storage | todo | Works in ReChord (full speed, SD read-only) |

### Ours

| Feature | Status | Notes |
|---|---|---|
| Cover Flow album browser | sim | Perspective covers from a 69 KB cache, by album artist |
| Album favorites | sim | Hold Play on an album; Music → Favorites |
| Configurable home screen | sim | Start screen setting: Main menu, Now Playing, Albums, Artists, Favorites, Playlists |
| M3U playlists | sim | Resolved by the PC indexer, including MusicBee's Windows paths |

### Later

| Feature | Status | Notes |
|---|---|---|
| Parametric EQ | todo | ReChord's DSP chain has an EQ module (host-tested) |
| ReplayGain | todo | ReChord's player supports track/album (host-tested) |
| Lyrics (.lrc) | todo | |
| USB DAC mode | todo | |
| Bluetooth | todo | External module with its own stack: GPIO2.7 power, UART1 115,200 ASCII commands, audio likely analog from the DAC (facts 13) |

## Platform

| Piece | Status | Notes |
|---|---|---|
| Simulator (SDL2, 320×170 RGB565 at 3×, keyboard keys) | sim | Frame stats in the window title; `--headless --frames N --screenshot` for automated checks |
| HAL: display, input, clock, files, audio, power | sim | Display power and brightness, audio gain and output, battery and power-off added in 1e |
| Key gestures: press, long press, repeat | sim | Tested in `core` |
| Strip renderer (no full framebuffer) | sim | 320×34 strips |
| Quality gates in ctest: no comments, clang-format, Cortex-M3 RAM/heap audit | done | The audit also reports flash per module |
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

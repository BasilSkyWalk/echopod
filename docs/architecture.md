# Architecture

Custom firmware for the Snowsky Echo Mini. The app layer is portable C. It runs in a macOS simulator during development and on the device on top of a thin hardware layer.

## Layers

```
platform/sim        platform/device
        \                  /
         implement  hal  (interfaces only)
                     |
                    app          composition root: theme, screens, player, stores, main step
           /     /       \        \
         ui   fonts    settings   hal
           \   /           |
            gfx        playback
              \            |
               \        library
                \         |
                   core
```

| Layer | Path | May depend on | Purpose |
|---|---|---|---|
| core | `src/core` | nothing | Pure logic: deadlines, key gestures, frame stats, UTF-8, string building, formatting, hashing, seeded random, the power policy |
| gfx | `src/gfx` | core | RGB565 colors and blending, rectangles, clipped canvases, strip rendering, 4-bit anti-aliased text, images drawn into perspective quads |
| fonts | `src/fonts` | gfx | Generated glyph tables (see Generated code) |
| ui | `src/ui` | core, gfx | Screen interface and stack, navigation intents, list view, menus, choice screens, messages, toasts, header and status icons, labels, marquee, fades, Cover Flow layout and motion, theme type |
| library | `src/library` | core | Byte sources, a 4×512 B block cache, the library database reader, browse queries, the favorites set and the covers file reader (see `docs/library-format.md`) |
| codec | `src/codec` | core | FLAC decoding (up to 2 channels, 24 bits, 4,608-sample blocks) from a read callback, with header and frame CRC checks |
| playback | `src/playback` | core, library | Play queue (list order, songs shuffle, album shuffle, repeat, gapless look-ahead) and the resume file format |
| settings | `src/settings` | core, playback | The settings values, their defaults and option tables, and the settings file format |
| hal | `src/hal` | core (types only) | Hardware interfaces: display (with power and brightness), input, clock, files, audio (with gain and output), power (battery, power off), system (firmware tag, CPU clock) |
| app | `src/app` | core, gfx, fonts, ui, library, playback, settings, hal | Default theme, screen tree, library, settings and favorites stores, cover cache, player controller, browse, Cover Flow, Settings and Now Playing screens, frame overlay, the main step |
| platform | `platform/*` | app, hal, codec, OS/SDK | Implements hal and owns `main()` |

The CMake targets enforce these rules. Each module exposes only its own `include/<module>/` directory, and a module can only include headers from targets it links. Anything in a module's own directory, outside `include/`, is private.

## Screens and input

- A `Screen` is a title plus callbacks: `enter`, `handle_key`, `update` and `draw`. `handle_key` returns a `ScreenAction` (none, push, pop, or pop to root), and the app applies it to the `ScreenStack`. Screens never reach into the stack themselves.
- Raw key transitions come from `hal`. `core/key_tracker` turns them into gestures (press, long press, repeat), and `ui/navigation` maps gestures to intents (previous, next, select, back, home). Remapping buttons is a change in one place.
- Each key has a gesture type: tap-or-hold, repeat (press on down, then repeats), or tap-hold-repeat (tap on release, long press, then repeats). The volume keys repeat, except on screens that set `hold_volume_keys` (Now Playing), where they're tap-hold-repeat so a hold can skip tracks. The app switches the type whenever the top screen changes.
- Screen instances are static and owned by the app's `AppScreens`. There is no heap.
- A long Play is `NAVIGATION_OPTIONS`. Lists hand it to their `hold` action; in browse lists it toggles an album's favorite.
- Smart Play: in a browse list, ▶‖ on the track the player already has loaded (playing or paused) pushes Now Playing without restarting it. ≡ pushes Now Playing from any screen while the player has a track, and does nothing on Now Playing itself.
- Settings is a menu whose rows show their current value. Every row opens the same `ChoiceScreen`, re-initialized from a table of `SettingChoice` entries (title, options, current, choose), and the current option gets a ✓.

## Power

- `core/power_policy` decides what keys mean for power. A tap of Power turns the screen off or on, and a long Power asks for shutdown. With the screen off (after a tap or a timeout), ▶‖ and vol± are background keys: the app maps them as on Now Playing (tap vol± for volume, hold vol± to skip, ▶‖ to pause) and runs them without waking the screen. ≡ and ↩ do nothing. While the screen is off, the volume keys use tap-hold-repeat on every screen, so a hold skips. The policy also owns the screen timeout and the sleep timer.
- The app runs each key event through the policy before the screen sees it. When the policy turns the display on or off, the app calls `hal_display_set_power` and stops rendering while the screen is off. Settings are saved each time the screen goes off.
- Shutdown (long Power, or the sleep timer running out) saves the resume file and settings, turns the screen off and calls `hal_power_off()`. The simulator then quits; the device cuts power.

## Settings and favorites

- `app/settings_store` loads `/.echo/settings.bin` (defaults if it's missing or invalid), applies brightness, gain and output through the HAL, and only writes when the encoded bytes changed.
- At start-up the saved volume, shuffle and repeat go to the player first, and then a valid resume file overrides them. The current volume, shuffle and repeat are copied back into the settings before every save.
- The start screen setting pushes the matching path at start-up (for example Home → Music → Albums), so Back still leads home.
- `app/favorites_store` loads and saves `/.echo/favorites.bin` and toggles albums by key. Music → Favorites is a browse screen over the Albums view that only shows the positions of favorite albums, collected in album order whenever it's entered or changed. Favorite albums show ★ before their year.
- Toasts are app-owned. Screens get a pointer to the one `Toast` and show short messages such as "Added to Favorites". The app draws the toast on top of every screen.

## Library

- The PC indexer builds the database (`tools/library_index.py`) and writes it to `/.echo/library.db` on the card.
- On the device, `app/library_store` opens the database through `hal_file_*`, puts a block cache in front, and validates it with `library_open`.
- Browse queries (`library/library_browse`) answer three questions for any view: how many rows there are, what row N is, and what selecting it opens. They read records and strings on demand and never load the database into RAM.
- Browse screens come from a fixed pool of 6 levels. They share one cache of the visible rows' text, which is refreshed in `update` (never during `draw`) whenever the visible rows change.

## Playback

- `hal/audio.h` is high level, as the brief asked. `play(path, start, token)`, `queue_next(path, token)`, pause, resume, seek, volume and status. The backend decodes; the device backend will wrap ReChord's player.
- Gapless: when a track starts, the player queues the next one with a new token. The backend switches without a gap and reports the new token, and `player_update` then moves the queue on and queues the following track.
- Shuffle is seeded (`core/random`). The resume file (`/.echo/resume.bin`, `playback/resume_state`) stores the queue plan (source list, anchor track, shuffle mode, seed), the cursor, the track and the position, so the exact same queue is rebuilt after power-off. A library fingerprint rejects resume files from an older index.
- The player saves on start, on track change, on pause, every 15 s while playing, and in `app_stop()`.

## Cover Flow

- Now Playing shows the playing album's full cover (112 px) on the left, below the header, with the text, progress and times in a column to its right. It loads the cover through the same `CoverCache` full slot in `update`. Without a cover it keeps the full-width text layout.

- Albums run by album artist, then by year, the same order as Album Artists. The screen keeps up to 1,024 album indices (2 KB).
- `ui/cover_flow` holds the geometry and the motion, in fixed point (256 = one cover). The center cover is flat and 112 px. Neighbours are 42 px wide trapezoids, 84 px tall on the side facing the center and 64 px on the far side, spaced 32 px apart. Between those positions, every measure is interpolated, so a cover grows and flattens as it slides in. The position eases toward the selected album with a 70 ms time constant, at no less than about 4 covers per second, and never overshoots.
- Covers are drawn farthest first, so nearer covers overlap them. `gfx/image` draws a cover column by column with nearest-neighbour sampling, clipped to the current strip.
- `app/cover_cache` holds 7 half-size covers (56 px, 6,272 B each) for the center ±3, in slot `position % 7`, plus one full-size cover (112 px, 25,088 B) for the center, about 69 KB in total.
- Covers load in `update`, one per frame: the nearest missing half cover first, then the full cover once the motion has stopped. A cover is drawn from the full image when that's loaded for its album, otherwise from the half image, otherwise as a placeholder. Beyond ±3, covers that aren't loaded aren't drawn.
- Play opens the album's track list, a long Play toggles the favorite, and Back leaves. The screen is full screen: no header or status icons, and the title (★ for favorites) and album artist go under the cover. The first time it opens, it starts at the playing album.

## Rendering

The panel keeps its own image memory, so we never hold a full framebuffer. The strip renderer clears a 320×34 buffer, asks the current screen to draw into it, pushes it to the display, and moves on to the next strip. Before pushing, `gfx/frame_diff` compares a hash of every row (FNV-1a over pixel pairs) with the previous frame's (680 bytes for 170 rows), and only the span of changed rows in each strip goes to the display. An unchanged frame sends nothing, and moving a selection sends a few rows. `app_start` forgets the hashes, so the first frame is always sent whole. After the screen, the app draws the status icons (play state on the left of the header, battery on the right, both inside the header insets), the toast and the frame overlay. The overlay shows the average frame time and its split: u (input, player and screen update, including card reads), d (drawing and hashing) and s (sending to the panel). The renderer clears every strip to the theme background, so screens don't fill it again. Screens marked `full_screen` (the key test) get no status icons. While the start-up or goodbye fade runs, the app draws only the fade (centred text in the title font, blended from the background to the text colour) and drops key events. `app_show_splash` starts the 1.1 s "Echo Mini" fade after `app_start`. While a track plays, the app pushes Now Playing after 10 s without a key press and when the screen turns on, unless the top screen sets `stays_when_idle`. A long Power press saves the session at once, shows "Goodbye" fading out over 1 s, then turns off. With the screen off it turns off straight away. Drawing uses screen coordinates and clips to the strip. Lists skip rows outside the strip, and glyphs outside the clip are culled. This saves ~85 KB of RAM compared with a full framebuffer.

## Generated code

- `src/fonts/` is generated by `tools/font_build.py` from `assets/fonts/fonts.json` and the TTFs in `assets/fonts/`. Never edit it by hand.
- Fonts are Inter (SIL OFL 1.1, license in `assets/fonts/inter/OFL.txt`), pre-rendered to 4-bit coverage. Coverage is ASCII, Latin-1 and common punctuation.
- Regenerate with:

  ```sh
  .venv/bin/python tools/font_build.py --config assets/fonts/fonts.json --output src/fonts --style .clang-format
  ```

- The `fonts_up_to_date` test fails if the committed output differs from a fresh build.

## Memory

- No heap in `src/`. Every buffer is static and sized at compile time.
- `src/` is freestanding C11 and includes only `<stdint.h>`, `<stdbool.h>` and `<stddef.h>`.
- The `target_audit` test cross-compiles `src/` for Cortex-M3 and reports flash and static RAM per module. It fails on any heap symbol and when static RAM exceeds `ECHO_APP_RAM_BUDGET_BYTES`.
- No recursion and no VLAs. The device stack is 16 KB.

## Code rules

- No comments in any code: C, CMake or Python. Names and small functions carry the intent. Prose belongs in `docs/`.
- Names:

  | Kind | Style | Example |
  |---|---|---|
  | Types | `PascalCase` | `KeyTracker`, `FrameStats` |
  | Functions and variables | `snake_case`; public functions prefixed with the module | `key_tracker_update` |
  | Constants and enum values | `UPPER_SNAKE_CASE`, prefixed | `KEY_GESTURE_PRESS`, `FONT_INTER_REGULAR_16` |
  | Files | `snake_case`, named after the module | `key_tracker.c` |

- Functions:
  - do one thing
  - stay short (aim for under 20 lines)
  - take at most 4 parameters; group anything more into a struct (`Label`, `TextStyle`, `MenuConfig`)
- Pass small value types by value and anything larger by pointer. Use `const` wherever data is read-only.
- No mutable globals in `src/`, except state owned by the composition root (`app.c`). Platform singletons live in `platform/`.
- Every rule in `core`, `gfx` and `ui` has a unit test. Test names describe the behaviour.
- `clang-format` (`.clang-format`) defines formatting. Warnings are errors (`cmake/project_options.cmake`).

## Testing

`ctest` runs:

| Test | Checks |
|---|---|
| unit tests | one executable per module test file under `tests/`; screens are tested by drawing into canvases and checking pixels |
| `test_app` | the composed app through a fake HAL: navigation, frame output, overlay |
| `sim_headless_smoke` | that the simulator runs headless and saves a screenshot |
| `tools_unit_tests` | Python tests for the PC tools |
| `fonts_up_to_date` | that the generated fonts match their sources |
| `library_fixture_up_to_date` | that `tests/fixtures/small_library.db`, written by the Python indexer and read by the C tests, matches the indexer |
| `cover_fixture_up_to_date` | that `tests/fixtures/small_covers.bin` (two solid-color covers for the fixture albums) matches the cover exporter |
| `no_comments` | that no code file contains a comment |
| `format` | that every C file matches `clang-format` |
| `target_audit` | the Cortex-M3 cross-compile: heap symbols, flash and static RAM |
| device unit tests | `test_key_ladder`, `test_key_scanner`, `test_panel`, `test_sd_card`, `test_tick_clock` and `test_fat`: the device driver logic, run on the Mac. `test_fat` reads `tests/fixtures/cards/*.sparse`: FAT32 (MBR), FAT16 (unpartitioned) and exFAT (MBR) card images that macOS formatted and filled (`tools/fat_fixture.py`, run by hand; the images aren't reproducible) |
| `device_build` | that `cmake --preset device` configures and builds the device ELF, and the image when the reference file is present |

Host builds run under UndefinedBehaviorSanitizer. AddressSanitizer is opt-in (`ECHO_ADDRESS_SANITIZER`) because it hangs at startup on macOS 27 with Apple clang 17.

## Device platform

Written only from `docs/device-facts.md` (the clean-room rule in `NOTES.md`).

- `platform/device/logic` holds the driver logic that doesn't touch hardware, so it builds and is tested on the Mac too:
  - `key_ladder`: ADC bands to keys
  - `key_scanner`: debounce over two samples, a transition queue, and ignoring the Power press still held from switching on
  - `panel`: the init streams for both panel variants, and the window and pixel commands, sent through a `PanelBus`
  - `tick_clock`: milliseconds and microseconds from the 24-bit SysTick down-counter
  - `sd_card`: the SD setup protocol (v1 and v2 cards, the ACMD41 retries, the RCA, capacity from the CSD register, the 4-bit bus), driven through an `SdHost`
  - `emmc_layout`: finds the firmware slots (from the IDB's system size) and scans for the user volume on internal storage. It also answers which internal-storage writes are allowed: only the user volume and the two firmware slots, never the IDB, the SAVE block or the gap
  - `usb_device`: the descriptors and standard requests for a full-speed, bulk-only mass-storage device (`1209:0001`, the pid.codes test ID), plus GET_MAX_LUN and the mass-storage reset
  - `msc`: the bulk-only transport and SCSI commands over up to two LUNs. A reply shorter than the host asked for is padded with zeros and reported in the residue instead of stalling. Ejecting a LUN marks only that LUN absent
  - `image_crc` and `updater`: the image check and the slot writer. The check accepts only our 5 MiB shape (size, magic, fw size `0x44C000`, header copy, trailer CRC, fits a slot) and reports an image already in slot 1 as installed. The install fills slot 2, then copies slot 2 into slot 1. For each slot it first writes `0xFF` over the first 8 KiB, then every other 8 KiB chunk, then the first chunk last. Chunks that already match are skipped, and every write is read back. One slot is complete at every moment
  - `fat`: a read-only FAT16, FAT32 and exFAT reader over a `BlockDevice`. It reads unpartitioned cards or MBR partition 0, follows FAT long names and exFAT name entries (UTF-16, including surrogate pairs), matches path components exactly by code point and ASCII letters in either case, and reads at any offset. FAT files follow a per-file cluster cursor; exFAT files marked NoFatChain are contiguous and skip the FAT. Cluster walks and directory scans are bounded, so a corrupt chain can't hang the player
- `platform/device/board` is device-only. It holds the register helpers, GPIO and pin mux, startup (the vector tables and the steps in facts 3.1), the linker script, a minimal `memcpy`/`memset`, and the HAL implementations.
- **Build:** `cmake --preset device && cmake --build --preset device` cross-compiles to `build/device/platform/device/echo_device.elf`. Then `tools/make_image.py` packs it into `build/device/HIFIEC90.IMG`, using header bytes from the RECHO 0.9.2 file at `ECHO_REFERENCE_IMG` (outside the repo). Without that file, only the ELF is built.
- **Image rules,** enforced by the linker script and the builder:
  - `.data` holds only the 8-byte entry vector; no initialised mutable globals.
  - Code stays below the reference's row-0 end.
  - `.bss` ends below `0x0304C000`.
  - Nothing loaded is left out of the image.
- **Run model:** fully polled, with interrupts off. SysTick runs freely at the 24 MHz core clock, wrapping about every 0.7 s, so the clock has to be read at least that often. The display driver samples the keys before and after every strip it pushes, so key presses aren't missed during slow frames.
- **Storage:** `board/mmc` drives the DW-MMC controller (commands, polled single-block reads and writes, the card clock) and `board/sd` powers the SD slot and checks card detect. `board/emmc` starts internal storage (8-bit bus at 24 MHz) on first use and refuses any write outside what `emmc_layout` allows. The file HAL mounts the card (FAT16, FAT32 or exFAT) on the first open and can hold 8 files open at once; it reads only for now.
- **USB:** the main loop watches VBUS. When the cable is plugged in, the player shows "Cable detected", then `board/usb_session` powers the PHY and the polled DWC2 core (`board/usb`) and waits up to 3 s for a host; no host means a charger, and the app carries on. With a host:
  - the app shows a notice, and its steps pause
  - LUN 0 is the internal-storage user volume and LUN 1 is the SD card, both read/write. Sense data is kept per LUN
  - if no computer answers, or the controller fails to start, `board/usb_report` shows three notices (a key press moves on): the core ID and OTG control after power-up; the failed init step (with `0x10` set when internal storage started) and GHWCFG4; then GINTSTS and OTG control after the wait. This is temporary, while USB is brought up on hardware
  - the session ends when every LUN that was present has been ejected, or the cable is pulled. If the session wrote to the card, the file HAL forgets its mount and open files, and `app_start` reloads the app from the changed card. Then `board/update` looks for `/HIFIEC90.IMG` on internal storage and installs it if it's valid and differs from slot 1, showing progress and the result (a key press dismisses it). Finally `app_wake` restarts the screen timeout. The new firmware starts at the next power cycle, because a warm reboot doesn't work on this hardware
- **Firmware tag:** `hal_system_version` (`hal/system.h`) reads the build tag from slot 1's header on the device and returns "simulator" in the simulator. Diagnostics shows it as Firmware.
- **CPU clock:** `hal_system_clock_mhz` and `hal_system_set_clock_mhz` (`hal/system.h`). On the device, `board/clock_switch.c` follows stock's PLL order (`docs/device-facts.md` 4.5) with the register words from `device/pll` (96 and 200 MHz). It waits up to 200 ms for lock and stays at 24 MHz without it. After a switch it rescales the SysTick clock (`tick_clock_set_rate`). The clock isn't saved, so every start is at 24 MHz. SysTick wraps every 175 ms at 96 MHz, so the clock must be read at least that often. Diagnostics → CPU clock switches between 24 and 96 MHz, and Diagnostics → Stopwatch counts seconds to check the rate against a phone. CPU clock cycles 24 → 96 → 200 → 24 MHz.
- **Test tone:** `hal_audio_test_tone` (`hal/audio.h`) blocks for about 5 s. On the device, `board/dac.c` runs the SoC side of `docs/device-facts.md` 11 (gates, I2C0, ACODEC mux, I2S1, polled FIFO feed) and `device/dac_sequence` runs the CS43131 side (power pins, probe, register sequence, output on and off) through a `DacBus`. It plays one sine period of 100 samples (441 Hz at a true 44.1 kHz) at amplitude 9000 (ReChord's level, about −11 dBFS): 2 s with the I2S divider from the facts formula (MCLK / (64 × rate) − 1, truncated, which gives 2), then 2 s with it rounded (3). It reports No DAC, Setup failed, Not ready or I2S stalled when a step fails. The simulator returns Played.
- **DMA tone:** `hal_audio_dma_tone_start`, `_stop` and `_status`. On the device it checks COMP_TYPE at `0x600003F8`, brings up the same audio path with the rounded divider and INTCR `0x01F00100` and without XFER, then runs DMA channel 5 (`board/dmac.c`) over a 4,000-word sine buffer (40 periods, L + R × 2¹⁶ per word) through one linked-list item whose LLP points at itself (`device/dma_loop`), with every channel interrupt masked. Then DMACR = `0x110` and XFER = 1. The call returns at once, and Diagnostics → DMA tone shows "Playing" with channel 5's read position, "Ended" if the channel stopped by itself, or the failed step. Pressing it again suspends the channel, disables it, and closes the audio path. A clock switch suspends and resumes the channel around it (`docs/device-facts.md` 11.11).
- **Stubs for now:** file writes from the app, audio, battery (unknown), and brightness (backlight on or off only).

## Simulator audio

- Windowed mode plays through SDL audio. dr_flac, dr_mp3 and dr_wav decode (vendored, pinned in `third_party/dr_libs/COMMIT`). Each track feeds an `SDL_AudioStream` that resamples to the 48 kHz device rate. Two decoder slots (current and queued) make the hand-over gapless.
- Headless mode uses a silent virtual backend that advances on the simulated clock, so scripted runs, track transitions and resume are deterministic.
- `SDL_AUDIODRIVER=dummy` runs the real-time path without sound.
- Third-party code lives in `third_party/` and compiles without our warning flags. The comment and format checks skip it.

## Simulator storage

`--card DIR` maps the card root `/` to a host folder, which is only ever read. `--system-dir DIR` maps `/.echo/` to a separate folder, so the database, covers, resume file, settings and favorites can live in `build/` without touching your music folder. Writes are refused outside `--system-dir`:

```sh
.venv/bin/python tools/library_index.py --music ~/Music --output build/library/library.db
build/dev/platform/sim/echo_sim --card ~/Music --system-dir build/library
```

## Simulator power and display

- `hal_display_set_power(false)` blanks the window. Brightness dims the window (level 1 is about a quarter of full), but screenshots hold the undimmed panel.
- The battery is fixed: 80 % by default, or `--battery N` and `--charging`.
- Gain and output are stored but don't change the sound.
- `hal_power_off()` ends the run loop, and the simulator exits normally.

## Simulator automation

In headless mode the simulator uses a virtual clock (16 ms per frame), so runs are deterministic. `--keys` plays a comma-separated script (`up`, `down`, `play`, `menu`, `back`, `power`, each optionally `:long`) for driving screens in screenshot runs. For example:

```sh
build/dev/platform/sim/echo_sim --headless --frames 120 --keys down,down,down,play --screenshot shot.bmp
```

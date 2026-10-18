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
| codec | `src/codec` | core | FLAC decoding (up to 2 channels, 24 bits, 4,608-sample blocks) from a read callback, with header and frame CRC checks, and seeking by bisection on frame headers |
| playback | `src/playback` | core, library | Play queue (list order, songs shuffle, album shuffle, repeat, gapless look-ahead) and the resume file format |
| settings | `src/settings` | core, playback | The settings values, their defaults and option tables, and the settings file format |
| hal | `src/hal` | core (types only) | Hardware interfaces: display (with power and brightness), input, clock, files, audio (with gain and output), power (battery, power off), system (firmware tag, CPU clock), Bluetooth (module power, scan, pair, disconnect, status) |
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
- Lists draw the selected row as a highlight line over a gradient, a › chevron on rows that open another screen (`ListModel.opens`), and a slim inset scrollbar thumb. Menu items can be hidden by a `shown` callback, and a menu can give the right of its content to a side panel (`MenuPanel`). Home uses that for `home_art`: the playing album's cover with a reflection, or library covers cycling every 5 s when nothing is loaded; Home lists Now Playing last and only while the player has a track.
- Settings is a menu whose rows show their current value. Every row opens the same `ChoiceScreen`, re-initialized from a table of `SettingChoice` entries (title, options, current, choose), and the current option gets a ✓.

## Power

- `core/power_policy` decides what keys mean for power. A tap of Power turns the screen off or on, and a long Power asks for shutdown. With the screen off (after a tap or a timeout), ▶‖ and vol± are background keys: the app maps them as on Now Playing (tap vol± for volume, hold vol± to skip, ▶‖ to pause) and runs them without waking the screen. ≡ and ↩ do nothing. While the screen is off, the volume keys use tap-hold-repeat on every screen, so a hold skips. The policy also owns the screen timeout and the sleep timer.
- The app runs each key event through the policy before the screen sees it. When the policy turns the display on or off, the app calls `hal_display_set_power` and stops rendering while the screen is off. Settings are saved each time the screen goes off.
- Shutdown (long Power, or the sleep timer running out) saves the resume file and settings, turns the screen off and calls `hal_power_off()`. The simulator then quits; the device cuts power.
- Low battery (`core/low_battery`, stock's rule): every 250 ms the app reads the battery, and if it stays below 3450 mV for 2 s without USB power, it shuts down the same way with "Battery low" in place of "Goodbye".
- CPU clock: 200 MHz with the screen on. With the screen off, 96 MHz while audio is up and 24 MHz once `HalAudioStatus.idle` says the output is powered down. The device's audio HAL raises the clock to 96 MHz itself before starting output, and a USB session starts at 96 MHz at least.
- On the device, a pause that lasts 30 s parks the audio: the engine, the DAC path and core 1 stop, and the status keeps reporting the paused track and position. Resume or seek restarts it from there, as after a clock switch.
- Brightness 1–10 maps to a backlight duty of 8, 12, 17, 23, 30, 39, 50, 63, 80 and 100 % (`device/backlight`). Below 100 % GPIO2.4 runs stock's 10 kHz PWM, whose period follows PCLK and is reprogrammed after every clock switch.

## Settings and favorites

- `app/settings_store` loads `/.echo/settings.bin` (defaults if it's missing or invalid), applies brightness, gain and output through the HAL, and only writes when the encoded bytes changed.
- On the device, `hal_file_write` overwrites an existing file in place from offset 0 (`fat_write`, FAT16, FAT32 and exFAT) and fails if the file is missing or too short; it never allocates clusters or changes a directory entry. `tools/save_files.py` makes 4 KB zero-filled `settings.bin`, `resume.bin` and `favorites.bin` once. Every reader checks its magic and accepts a longer file, so a zero-filled file reads as missing.
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

The panel keeps its own image memory, so we never hold a full framebuffer. The strip renderer clears a 320×34 buffer, asks the current screen to draw into it, pushes it to the display, and moves on to the next strip. Before pushing, `gfx/frame_diff` compares a hash of every row (FNV-1a over pixel pairs) with the previous frame's (680 bytes for 170 rows), and only the span of changed rows in each strip goes to the display. An unchanged frame sends nothing, and moving a selection sends a few rows. `app_start` forgets the hashes, so the first frame is always sent whole. After the screen, the app draws the status icons (play state on the left of the header, battery on the right, both inside the header insets), the toast and the frame overlay. The overlay shows the average frame time and its split: u (input, player and screen update, including card reads), d (drawing and hashing) and s (sending to the panel). The renderer clears every strip to the theme background, so screens don't fill it again. Screens marked `full_screen` (the key test) get no status icons. While the start-up or goodbye fade runs, the app draws only the fade (centred text in the title font, blended from the background to the text colour) and drops key events. `app_show_splash` starts the 1.1 s "Echo Mini" fade after `app_start`. While a track plays, turning the screen on pushes Now Playing, unless the top screen sets `stays_on_wake`. Idling in a menu never leaves it. Pushing or popping a screen by key slides it (`ui/slide`, 240 ms, cubic ease-out): the title bar stays and switches its title halfway, and both screens are drawn shifted below it through `canvas_shifted`; if either screen is full screen, the whole screen slides. Waking into Now Playing doesn't slide. Browse levels keep two row caches so both sides of a slide have their text. A long Power press saves the session at once, shows "Goodbye" fading out over 1 s, then turns off. With the screen off it turns off straight away. Drawing uses screen coordinates and clips to the strip. Lists skip rows outside the strip, and glyphs outside the clip are culled. This saves ~85 KB of RAM compared with a full framebuffer.

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
  - `bt_module`: the Bluetooth module's command protocol (facts 13.4). It waits up to 5 s for the `CV1` ready report, sends `CS00` to scan (once more after 4 s if nothing answers, `CP00` after 30 s), collects `CS` scan results (up to 6, dropping unnamed and repeated addresses) until `CSEN`, and sends `CQ00` every 3 s while ready or connected. Connecting sends `CL06` + the picked address (once more as `CH06` after 4 s if nothing answers, `CT00` and a failure after 30 s); `CLOK` means paired, `CLER`/`CHER` failed, `CHOK` connected, `CQO` + address + name connected, any other status letter while connected or `CT`/`CTOK` means the link dropped. Disconnecting sends `CT00`. While connected, starting the stream sends `CY0` + `1E` + 0 and, 100 ms later, the play notice `C` `86` `00` + 0; stopping it sends `CY0` + `00` + 0 and then `C` `87` `00` + 0 (facts 13.4). A dropped link ends the stream. While the stream is on, `board/bluetooth.c` holds GPIO2.0 high and GPIO2.14 low on every service pass, as stock does on connect (facts 13.5), and restores them when the stream stops or the module powers off. Received bytes are split into frames as soon as a frame is complete; an incomplete frame is dropped after 2 ms of silence. It also keeps the last 12 bytes and counts bytes and frames
  - `usb_device`: the descriptors and standard requests for a full-speed, bulk-only mass-storage device (`1209:0001`, the pid.codes test ID), plus GET_MAX_LUN and the mass-storage reset
  - `msc`: the bulk-only transport and SCSI commands over up to two LUNs. A reply shorter than the host asked for is padded with zeros and reported in the residue instead of stalling. Ejecting a LUN marks only that LUN absent
  - `image_crc` and `updater`: the image check and the slot writer. The check accepts only our 5 MiB shape (size, magic, fw size `0x44C000`, header copy, trailer CRC, fits a slot) and reports an image already in slot 1 as installed. The install fills slot 2, then copies slot 2 into slot 1. For each slot it first writes `0xFF` over the first 8 KiB, then every other 8 KiB chunk, then the first chunk last. Chunks that already match are skipped, and every write is read back. One slot is complete at every moment
  - `fat`: a read-only FAT16, FAT32 and exFAT reader over a `BlockDevice`. It reads unpartitioned cards or MBR partition 0, follows FAT long names and exFAT name entries (UTF-16, including surrogate pairs), matches path components exactly by code point and ASCII letters in either case, and reads at any offset. FAT files follow a per-file cluster cursor; exFAT files marked NoFatChain are contiguous and skip the FAT. Cluster walks and directory scans are bounded, so a corrupt chain can't hang the player
- `platform/device/board` is device-only. It holds the register helpers, GPIO and pin mux, startup (the vector tables and the steps in facts 3.1), the linker script, a minimal `memcpy`/`memset`, and the HAL implementations.
- `platform/device/core1` is the program for the second core: its own startup, `main` and linker script (code at core 1 address 0, data at `0x01021000`, stack top `0x01024000`, facts 3.2). It builds to `core1.bin`, which is linked into the core 0 image as read-only data. `device/core1_control` holds the power-up, start and stop sequences and the shared-block layout, tested on the Mac; `board/core1.c` runs them.
- **Build:** `cmake --preset device && cmake --build --preset device` cross-compiles to `build/device/platform/device/echo_device.elf`. Then `tools/make_image.py` packs it into `build/device/HIFIEC90.IMG`, using header bytes from the RECHO 0.9.2 file at `ECHO_REFERENCE_IMG` (outside the repo). Without that file, only the ELF is built.
- **Image rules,** enforced by the linker script and the builder:
  - `.data` holds only the 8-byte entry vector; no initialised mutable globals.
  - Code stays below the reference's row-0 end.
  - `.bss` ends below `0x0304C000`.
  - Nothing loaded is left out of the image.
- **Run model:** fully polled, with interrupts off. SysTick runs freely at the 24 MHz core clock, wrapping about every 0.7 s, so the clock has to be read at least that often. The display driver samples the keys before and after every strip it pushes, so key presses aren't missed during slow frames.
- **Storage:** `board/mmc` drives the DW-MMC controller (commands, polled single-block reads and writes, the card clock) and `board/sd` powers the SD slot and checks card detect. `board/emmc` starts internal storage (8-bit bus at 24 MHz) on first use and refuses any write outside what `emmc_layout` allows. The file HAL mounts the card (FAT16, FAT32 or exFAT) on the first open and can hold 8 files open at once; it reads only for now.
- **USB:** the main loop watches VBUS. When the cable is plugged in, `board/usb_session` powers the PHY and the polled DWC2 core (`board/usb`) and connects, without touching audio or the screen. The main loop then keeps running and checks GINTSTS for a bus reset on every pass, for up to 3 s, with the CPU clock held at 96 MHz or more. No reset means a charger: USB powers down quietly and the player carries on, playing and charging. With a host:
  - the app shows "Computer found", the audio power pins are silenced, and its steps pause
  - LUN 0 is the internal-storage user volume and LUN 1 is the SD card, both read/write. Sense data is kept per LUN
  - if the controller fails to start, `board/usb_report` shows four notices (a key press moves on): the core ID and OTG control after power-up; the failed init step (with `0x10` set when internal storage started) and GHWCFG4; then GINTSTS and OTG control after the wait. This is temporary, while USB is brought up on hardware
  - the session ends when every LUN that was present has been ejected, or the cable is pulled. If the session wrote to the card, the file HAL forgets its mount and open files, and `app_start` reloads the app from the changed card. Then `board/update` looks for `/HIFIEC90.IMG` on internal storage and installs it if it's valid and differs from slot 1, showing progress and the result (a key press dismisses it). Finally `app_wake` restarts the screen timeout. The new firmware starts at the next power cycle, because a warm reboot doesn't work on this hardware
- **Firmware tag:** `hal_system_version` (`hal/system.h`) reads the build tag from slot 1's header on the device and returns "simulator" in the simulator. Diagnostics shows it as Firmware.
- **CPU clock:** `hal_system_clock_mhz` and `hal_system_set_clock_mhz` (`hal/system.h`). On the device, `board/clock_switch.c` follows stock's PLL order (`docs/device-facts.md` 4.5) with the register words from `device/pll` (96 and 200 MHz). It waits up to 200 ms for lock and stays at 24 MHz without it. After a switch it rescales the SysTick clock (`tick_clock_set_rate`). The clock isn't saved, so every start is at 24 MHz. SysTick wraps every 175 ms at 96 MHz, so the clock must be read at least that often. Diagnostics → CPU clock switches between 24 and 96 MHz, and Diagnostics → Stopwatch counts seconds to check the rate against a phone. CPU clock cycles 24 → 96 → 200 → 24 MHz.
- **Test tone:** `hal_audio_test_tone` (`hal/audio.h`) blocks for about 5 s. On the device, `board/dac.c` runs the SoC side of `docs/device-facts.md` 11 (gates, I2C0, ACODEC mux, I2S1, polled FIFO feed) and `device/dac_sequence` runs the CS43131 side (power pins, probe, register sequence, output on and off) through a `DacBus`. It plays one sine period of 100 samples (441 Hz at a true 44.1 kHz) at amplitude 9000 (ReChord's level, about −11 dBFS): 2 s with the I2S divider from the facts formula (MCLK / (64 × rate) − 1, truncated, which gives 2), then 2 s with it rounded (3). It reports No DAC, Setup failed, Not ready or I2S stalled when a step fails; No DAC adds the I2C step (s1 start, s2 send, s3 NAK, s4 stop, s5 receive) and the I2C pending bits in hex. Diagnostics → Boot clocks shows the low halves of CRU `+0x10`, `+0x14` and `+0x18` as the ROM left them, read first thing in `board_main`. The simulator returns Played.
- **Device playback:** `board/audio.c` implements the audio HAL with `device/audio_engine` (host-tested). The engine opens a FLAC file through the file HAL and takes decoded frames from an `AudioSource`. On the device that is `device/core1_stream`: core 0 opens and seeks with its own decoder, streams the file into a 128 KB ring in core 1 RAM, and core 1 decodes frames into two slots; if core 1 doesn't start, `device/flac_source` decodes on core 0. Each frame is copied into a 16,000-word ring (L + R × 2¹⁶, 16-bit, rounded from 24-bit, software volume in 1 dB steps from level 50 down, 0 = mute) and keeps a list of segments (token, start frame, start position, rate) so the token and position follow what the DMA has played. DMA channel 5 loops over the ring through four linked items of 4,000 words (a DW block holds at most 4,088 items; `device/dma_loop` refuses more); the HAL turns its read address into a running count of consumed frames. The app calls `hal_audio_service()` at the start of each step and after each strip, so drawing can't starve the ring (16,000 frames is 167 ms at 96 kHz and 363 ms at 44.1 kHz). Supported rates are 44.1, 48, 88.2 and 96 kHz with the facts' ACODEC rows and the rounded I2S divider. A queued track at the same rate follows without a gap; a different rate waits for the ring to drain, then the path restarts at the new rate. Pause mutes the DAC and suspends the channel (nesting with the clock switch's suspend). Seek and play restart the ring. Three seconds after playback stops, the DAC path powers down. The device starts at 96 MHz (`START_CLOCK_MHZ`) right after reading the boot clocks.
- **DMA tone:** `hal_audio_dma_tone_start`, `_stop` and `_status`. On the device it checks COMP_TYPE at `0x600003F8`, brings up the same audio path with the rounded divider and INTCR `0x01F00100` and without XFER, then runs DMA channel 5 (`board/dmac.c`) over a 4,000-word sine buffer (40 periods, L + R × 2¹⁶ per word) through one linked-list item whose LLP points at itself (`device/dma_loop`), with every channel interrupt masked. Then DMACR = `0x110` and XFER = 1. The call returns at once, and Diagnostics → DMA tone shows "Playing" with channel 5's read position, "Ended" if the channel stopped by itself, or the failed step. Pressing it again suspends the channel, disables it, and closes the audio path. A clock switch suspends and resumes the channel around it (`docs/device-facts.md` 11.11).
- **Last start:** `hal_system_start_info` (`hal/system.h`). On the device, `device/start_record` keeps a record in `.noinit` RAM: the start count, the uptime (every 5 ms), a power-off mark, fault registers, the last battery reading (every 100 ms) and the last risky step (backlight on or off, clock switch, core 1 start or stop, card write) with its start time and whether it finished. Diagnostics → Last start, Last step and Last battery show what the previous run left, for example "#3 reset after 41 s", "In clock switch at 41 s" and "3712 mV".
- **Stall and fault screen:** a power loss wipes the Last start record, so a freeze that ends in a power cut reads Cold start. On the device the main loop beats a `device/stall_watch` each pass, and the 1 ms SysTick checks it every 5 ms. A loop silent for 10 s (paused during start-up, USB sessions, updates and power-off) trips it once: the interrupt silences the audio pins, clears the panel, turns the backlight full on and draws, in a 5×7 font scaled ×2 (`device/panic_text`), "STALL", the interrupted PC and LR (from SysTick's exception frame via `device_systick_entry`), and "UP <s> STEP <n> [U] MV <mV>" (U = the step never finished; step numbers follow `HalStartStep`). A fault draws "FAULT" with PC, LR and CFSR the same way. After either, holding Power for 2 s cuts power from the interrupt. Diagnostics → Stall test freezes the main loop on purpose to check the screen. A loop stuck with interrupts masked isn't caught.
- **Bluetooth:** `hal/bluetooth.h` powers the module, starts a scan and reports its state, the devices found and link counters. On the device, `board/bluetooth.c` follows facts 13.2–13.3: GPIO2.7, 2.16 and 2.17 are driven low at boot; power-on opens UART1 (group A, 115,200 8N1, divisor 13, FIFOs on, polled, no interrupts) and drives GPIO2.7 high; power-off drives all three low again, as does a module that never sends `CV1`. `hal_bluetooth_service` runs where `hal_audio_service` does (each step and after each strip): it drains the RX FIFO into `device/bt_module`, counts overruns, records the FIFO's peak level and its depth (from UART CPR), and sends any frame the protocol asks for. Diagnostics → Bluetooth powers the module on entry, scans once it's ready and powers it off on leaving. vol± moves the selection (›) through the devices found, ▶ pairs with the selected one (or scans when none were found, or turns the stream on and off while connected), and a long ▶ disconnects (or rescans when not connected). It shows the state (with the connected device's name, the version, the status letter and the pairing result), the link counters, the last bytes in hex and the devices, with ✓ on the connected one. Nothing is saved and the wired output isn't touched. The simulator reports ready after 300 ms, finds two devices over 3 s and connects 1.5 s after a pick.
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

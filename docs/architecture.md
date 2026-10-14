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
| ui | `src/ui` | core, gfx | Screen interface and stack, navigation intents, list view, menus, choice screens, messages, toasts, header and status icons, labels, marquee, Cover Flow layout and motion, theme type |
| library | `src/library` | core | Byte sources, a 4×512 B block cache, the library database reader, browse queries, the favorites set and the covers file reader (see `docs/library-format.md`) |
| playback | `src/playback` | core, library | Play queue (list order, songs shuffle, album shuffle, repeat, gapless look-ahead) and the resume file format |
| settings | `src/settings` | core, playback | The settings values, their defaults and option tables, and the settings file format |
| hal | `src/hal` | core (types only) | Hardware interfaces: display (with power and brightness), input, clock, files, audio (with gain and output), power (battery, power off) |
| app | `src/app` | core, gfx, fonts, ui, library, playback, settings, hal | Default theme, screen tree, library, settings and favorites stores, cover cache, player controller, browse, Cover Flow, Settings and Now Playing screens, frame overlay, the main step |
| platform | `platform/*` | app, hal, OS/SDK | Implements hal and owns `main()` |

The CMake targets enforce these rules. Each module exposes only its own `include/<module>/` directory, and a module can only include headers from targets it links. Anything in a module's own directory, outside `include/`, is private.

## Screens and input

- A `Screen` is a title plus callbacks: `enter`, `handle_key`, `update` and `draw`. `handle_key` returns a `ScreenAction` (none, push, pop, or pop to root), and the app applies it to the `ScreenStack`. Screens never reach into the stack themselves.
- Raw key transitions come from `hal`. `core/key_tracker` turns them into gestures (press, long press, repeat), and `ui/navigation` maps gestures to intents (previous, next, select, back, home). Remapping buttons is a change in one place.
- Each key has a gesture type: tap-or-hold, repeat (press on down, then repeats), or tap-hold-repeat (tap on release, long press, then repeats). The volume keys repeat, except on screens that set `hold_volume_keys` (Now Playing), where they're tap-hold-repeat so a hold can skip tracks. The app switches the type whenever the top screen changes.
- Screen instances are static and owned by the app's `AppScreens`. There is no heap.
- A long Play is `NAVIGATION_OPTIONS`. Lists hand it to their `hold` action; in browse lists it toggles an album's favorite.
- Settings is a menu whose rows show their current value. Every row opens the same `ChoiceScreen`, re-initialized from a table of `SettingChoice` entries (title, options, current, choose), and the current option gets a ✓.

## Power

- `core/power_policy` decides what keys mean for power: Power locks or unlocks, a long Power asks for shutdown, any key wakes a screen that timed out and is then consumed, and a locked player ignores every other key. It also owns the screen timeout and the sleep timer.
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

- Albums run by album artist, then by year, the same order as Album Artists. The screen keeps up to 1,024 album indices (2 KB).
- `ui/cover_flow` holds the geometry and the motion, in fixed point (256 = one cover). The center cover is flat and 112 px. Neighbours are 42 px wide trapezoids, 84 px tall on the side facing the center and 64 px on the far side, spaced 32 px apart. Between those positions, every measure is interpolated, so a cover grows and flattens as it slides in. The position eases toward the selected album with a 70 ms time constant, at no less than about 4 covers per second, and never overshoots.
- Covers are drawn farthest first, so nearer covers overlap them. `gfx/image` draws a cover column by column with nearest-neighbour sampling, clipped to the current strip.
- `app/cover_cache` holds 7 half-size covers (56 px, 6,272 B each) for the center ±3, in slot `position % 7`, plus one full-size cover (112 px, 25,088 B) for the center, about 69 KB in total.
- Covers load in `update`, one per frame: the nearest missing half cover first, then the full cover once the motion has stopped. A cover is drawn from the full image when that's loaded for its album, otherwise from the half image, otherwise as a placeholder. Beyond ±3, covers that aren't loaded aren't drawn.
- Play opens the album's track list, a long Play toggles the favorite, and Back leaves. The screen is full screen: no header or status icons, and the title (★ for favorites) and album artist go under the cover. The first time it opens, it starts at the playing album.

## Rendering

The panel keeps its own image memory, so we never hold a full framebuffer. The strip renderer clears a 320×34 buffer, asks the current screen to draw into it, pushes it to the display, and moves on to the next strip. After the screen, the app draws the status icons (play state on the left of the header, battery on the right, both inside the header insets), the toast and the frame overlay. Screens marked `full_screen` (the key test) get no status icons. Drawing uses screen coordinates and clips to the strip. Lists skip rows outside the strip, and glyphs outside the clip are culled. This saves ~85 KB of RAM compared with a full framebuffer.

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
| device unit tests | `test_key_ladder`, `test_key_scanner`, `test_panel`, `test_sd_card`, `test_tick_clock` and `test_fat`: the device driver logic, run on the Mac. `test_fat` reads `tests/fixtures/cards/*.sparse`, card images that macOS formatted and filled (`tools/fat_fixture.py`, run by hand; the images aren't reproducible) |
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
  - `fat`: a read-only FAT16 and FAT32 reader over a `BlockDevice`. It reads unpartitioned cards or MBR partition 0, follows long file names (UTF-16, including surrogate pairs), matches path components exactly by code point and ASCII letters in either case, and reads at any offset through a per-file cluster cursor
- `platform/device/board` is device-only. It holds the register helpers, GPIO and pin mux, startup (the vector tables and the steps in facts 3.1), the linker script, a minimal `memcpy`/`memset`, and the HAL implementations.
- **Build:** `cmake --preset device && cmake --build --preset device` cross-compiles to `build/device/platform/device/echo_device.elf`. Then `tools/make_image.py` packs it into `build/device/HIFIEC90.IMG`, using header bytes from the RECHO 0.9.2 file at `ECHO_REFERENCE_IMG` (outside the repo). Without that file, only the ELF is built.
- **Image rules,** enforced by the linker script and the builder:
  - `.data` holds only the 8-byte entry vector; no initialised mutable globals.
  - Code stays below the reference's row-0 end.
  - `.bss` ends below `0x0304C000`.
  - Nothing loaded is left out of the image.
- **Run model:** fully polled, with interrupts off. SysTick runs freely at the 24 MHz core clock, wrapping about every 0.7 s, so the clock has to be read at least that often. The display driver samples the keys before and after every strip it pushes, so key presses aren't missed during slow frames.
- **Storage:** `board/mmc` drives the DW-MMC controller (commands, polled single-block reads, the card clock) and `board/sd` powers the SD slot and checks card detect. The file HAL mounts the card on the first open and can hold 8 files open at once; it reads only for now.
- **Stubs for now:** file writes, audio, battery (unknown), and brightness (backlight on or off only).

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

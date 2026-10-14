# Architecture

Custom firmware for the Snowsky Echo Mini. The app layer is portable C. It runs in a macOS simulator during development and on the device on top of a thin hardware layer.

## Layers

```
platform/sim      platform/device (later)
      \                /
       implement  hal  (interfaces only)
                   |
                  app        composition root: wires core + gfx + hal, owns the main step
                 /   \
              core   gfx
```

| Layer | Path | May depend on | Purpose |
|---|---|---|---|
| core | `src/core` | nothing | Pure app logic: key gestures, frame stats, and later the library, queue and settings |
| gfx | `src/gfx` | nothing | Pixels, rectangles, canvases, strip rendering |
| hal | `src/hal` | core (types only) | Hardware interfaces: display, input, clock, and later fs, audio and power |
| app | `src/app` | core, gfx, hal | Screens and the main step |
| platform | `platform/*` | app, hal, OS/SDK | Implements hal and owns `main()` |

The CMake targets enforce these rules. Each module exposes only its own `include/<module>/` directory, and a module can only include headers from targets it links. Anything in a module's own directory, outside `include/`, is private.

## Rendering

The panel keeps its own image memory, so we never hold a full framebuffer. The strip renderer clears a 320×34 buffer, asks the current screen to draw into it, pushes it to the display, and moves on to the next strip. Drawing uses screen coordinates and clips to the strip. This saves ~85 KB of RAM compared with a full framebuffer.

## Memory

- No heap in `src/`. Every buffer is static and sized at compile time.
- `src/` is freestanding C11 and includes only `<stdint.h>`, `<stdbool.h>` and `<stddef.h>`.
- The `target_audit` test cross-compiles `src/` for Cortex-M3. It fails on any heap symbol and when static RAM exceeds `ECHO_APP_RAM_BUDGET_BYTES`.
- No recursion and no VLAs. The device stack is 16 KB.

## Code rules

- No comments in any code: C, CMake or Python. Names and small functions carry the intent. Prose belongs in `docs/`.
- Names:

  | Kind | Style | Example |
  |---|---|---|
  | Types | `PascalCase` | `KeyTracker`, `FrameStats` |
  | Functions and variables | `snake_case`; public functions prefixed with the module | `key_tracker_update` |
  | Constants and enum values | `UPPER_SNAKE_CASE`, prefixed | `KEY_GESTURE_PRESS` |
  | Files | `snake_case`, named after the module | `key_tracker.c` |

- Functions:
  - do one thing
  - stay short (aim for under 20 lines)
  - take at most 4 parameters; group anything more into a struct
- Pass small value types by value and anything larger by pointer. Use `const` wherever data is read-only.
- No mutable globals in `src/`, except state owned by the composition root (`app.c`). Platform singletons live in `platform/`.
- Every rule in `core` and `gfx` has a unit test. Test names describe the behaviour.
- `clang-format` (`.clang-format`) defines formatting. Warnings are errors (`cmake/project_options.cmake`).

## Testing

`ctest` runs:

| Test | Checks |
|---|---|
| unit tests | one executable per module test file under `tests/` |
| `no_comments` | that no code file contains a comment |
| `format` | that every C file matches `clang-format` |
| `target_audit` | the Cortex-M3 cross-compile: heap symbols and the static RAM budget |

Host builds run under UndefinedBehaviorSanitizer. AddressSanitizer is opt-in (`ECHO_ADDRESS_SANITIZER`) because it hangs at startup on macOS 27 with Apple clang 17.

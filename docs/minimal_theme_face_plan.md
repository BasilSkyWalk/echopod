# Implementation Plan: "Minimal" Now Playing Face (v2 Port)

This document details the complete design and implementation plan to integrate the **Now Playing v2** design into EchoPod's new **Now Playing Faces/Themes** subsystem (`src/np_faces/`) as the **"Minimal"** face.

---

## 1. Requirements & Core Decisions

1. **Integrated as a Native Face (`NP_FACE_MINIMAL`)**:
   - Accessed via **Settings > Display > Now Playing** $\to$ `Minimal`.
   - No separate flags, switches, or configuration file options.
   - Code and data live inside `src/np_faces/face_minimal.c`, using `.np_faces` lazy-loading macros to maintain 0 extra static RAM overhead on the device.
2. **Pure Black Background on Dark Theme**:
   - If the active theme is **Dark** (`theme->colors.background == COLOR_RGB(16, 17, 20)` or `choice.theme == THEME_DARK`), the Minimal face overrides the background with pure black `COLOR_RGB(0, 0, 0)`.
   - On other themes (Light, Fjord, Dusk, Paper, etc.), it uses `theme->colors.background`.
3. **"Shuffle Songs" $\to$ "Shuffle"**:
   - If playback mode is `Shuffle Songs` (`status->shuffle == SHUFFLE_SONGS`), render it simply as `"Shuffle"`.
   - If album shuffle is active, it continues to display `"Shuffle Albums"`.
4. **"Unknown Artist" Handling**:
   - If `frame->artist` is `"Unknown Artist"`, display blank space in the artist row instead of the label.
5. **Two-Line Word-Wrapped Title**:
   - Line 1 breaks cleanly at the last space before the boundary (no ellipsis).
   - Line 2 takes the remainder with truncation / ellipsis if needed. If the title fits on Line 1, Line 2 remains blank.
6. **Modes & Queue Position**:
   - Separate rows for playback modes and queue position (`"X of Y"`).
7. **Orientation Support (Landscape & Portrait)**:
   - **Landscape ($320 \times 170$)**: Fullscreen left cover art ($170 \times 170$), right column metadata, bottom full-width progress bar.
   - **Portrait ($170 \times 320$)**: Full-width cover art ($170 \times 170$) at the top, metadata stacked below it, and bottom full-width progress bar.

---

## 2. Layout & Coordinates Specification

### Landscape ($320 \times 170$)
```
+-------------------+------------------------------------+
|                   | [Status Bar: Icons & Clock]        |
|                   |                                    |
|                   | y=24: track_title (line 1)         |
|                   | y=46: track_title (line 2 / blank) |
|    Cover Art      | y=68: track_artist (blank if unk)  |
|   (170 x 170)     | y=90: track_album                  |
|                   |                                    |
|                   | y=117: playback_modes              |
|                   | y=133: queue position              |
|                   | y=134:                     bitrate |
|                   | y=149: elapsed / dur       [CODEC] |
+===================+====================================+
|============= Full-Width Progress Bar ==================|
+--------------------------------------------------------+
```
- Cover: $(x=0, y=0, w=170, h=170)$
- Right column: $x = 170 + 8 = 178$, width $= 320 - 178 - 8 = 134\text{ px}$.
- Modes and queue position are left-aligned in the right column.
- Bottom row ($y = 149$, height $14$):
  - `elapsed / duration` left-aligned at $x = 178$.
  - `[CODEC]` badge right-aligned at $x = 312$ and raised $1\text{ px}$.
  - `bitrate` right-aligned at $x = 312$, directly above the codec badge ($y = 134$).
- Progress bar: $y = 167$, $h = 3$, $w = 320$.

---

### Portrait ($170 \times 320$)
In portrait mode, the screen is $170\text{ px}$ wide and $320\text{ px}$ high:
```
+------------------------------------+
| [Status Bar: Icons & Clock]        |
|                                    |
|                                    |
|             Cover Art              |
|            (170 x 170)             |
|                                    |
|                                    |
+------------------------------------+
| y=176: track_title (line 1)        |
| y=198: track_title (line 2 / blank)|
| y=222: track_artist                |
| y=244: track_album                 |
|                                    |
| y=268: playback_modes              |
| y=284: queue position              |
| y=285:                     bitrate |
| y=300: elapsed / dur       [CODEC] |
+====================================+
|==== Full-Width Progress Bar =======|
+------------------------------------+
```
- **Cover Art**:
  - Pinned to the top: $(x=0, y=0, w=170, h=170)$.
  - Fills full width at the top. The status bar overlays transparently at the top edge.
- **Feedback Badge**:
  - Centered over the cover art at $x = (170 - 40)/2 = 65$, $y = (170 - 40)/2 = 65$.
- **Bottom Content Area ($y = 170$ to $317$)**:
  - Horizontal bounds: $x = 8\text{ px}$, width $= 170 - 16 = 154\text{ px}$.
  - $y = 176$: `track_title` Line 1 (height $20$)
  - $y = 198$: `track_title` Line 2 (height $20$, blank if 1-line)
  - $y = 222$: `track_artist` (height $20$, blank if `"Unknown Artist"`)
  - $y = 244$: `track_album` (height $20$)
  - $y = 268$: `playback_modes` (height $14$, left-aligned at $x = 8$, `"Shuffle"` instead of `"Shuffle Songs"`)
  - $y = 284$: `queue position` (height $14$, left-aligned at $x = 8$, e.g. `"3 of 12"`)
  - $y = 285$: `bitrate` (height $14$, right-aligned to $x = 162$, directly above the codec badge)
  - $y = 300$: Bottom row:
    - `elapsed / duration` left-aligned at $x = 8$; the elapsed time occupies a fixed `"00:00"` slot and the `/` plus duration start at fixed offsets, so the remaining time never shifts.
    - `[CODEC]` badge right-aligned at $x = 162$ and raised $1\text{ px}$.
- **Progress Bar**:
  - $y = 317$, height $= 3$, width $= 170$ across the bottom display edge.

---

## 3. Step-by-Step Implementation

### Step 1: Settings Engine & String Definition
1. In `src/core/include/core/strings_settings.def`:
   ```c
   STRING(STR_SETTINGS_MINIMAL, "Minimal")
   ```
2. In `src/settings/include/settings/settings.h`:
   ```c
   enum {
       NP_FACE_STANDARD,
       NP_FACE_CASSETTE,
       NP_FACE_VU_METERS,
       NP_FACE_TURNTABLE,
       NP_FACE_LCD,
       NP_FACE_MINIMAL,
       NP_FACE_COUNT,
       NP_FACE_OPTION_SLOTS = 8
   };
   ```
3. In `src/app/settings_screens.c`:
   ```c
   ECHO_RESIDENT static const StringId NP_FACE_OPTIONS[NP_FACE_COUNT] = {
       STR_SETTINGS_STANDARD, STR_SETTINGS_CASSETTE, STR_SETTINGS_VU_METERS, STR_SETTINGS_TURNTABLE,
       STR_SETTINGS_LCD_DECK, STR_SETTINGS_MINIMAL};
   ```

---

### Step 2: Implement `src/np_faces/face_minimal.c`

Create `src/np_faces/face_minimal.c` using `.np_faces` macros:
```c
#include "core/format.h"
#include "core/i18n.h"
#include "core/resident.h"
#include "core/string_builder.h"
#include "core/utf8.h"
#include "face_text.h"
#include "faces.h"
#include "gfx/image.h"
#include "gfx/text.h"
#include "ui/icons.h"
#include "ui/label.h"

// Enums and layout constants for Landscape & Portrait...
```

#### Key Logic Components:
1. **Pure Black Background for Dark Theme**:
   ```c
   ECHO_NP_FACES_CODE static Color background_color(const Theme *theme)
   {
       // Check if current theme is Dark theme (background is COLOR_RGB(16, 17, 20))
       if (theme->colors.background == COLOR_RGB(16, 17, 20)) {
           return COLOR_RGB(0, 0, 0);
       }
       return theme->colors.background;
   }
   ```
2. **Simplified Modes Text ("Shuffle")**:
   ```c
   ECHO_NP_FACES_CODE static void minimal_modes_text(const NowPlayingFrame *frame, char *buffer, size_t capacity)
   {
       if (frame->scrubbing) {
           StringBuilder builder = string_builder_make(buffer, capacity);
           string_builder_append(&builder, i18n_text(STR_MENU_SEEKING_HINT));
           return;
       }
       StringBuilder builder = string_builder_make(buffer, capacity);
       if (frame->status->shuffle == SHUFFLE_SONGS) {
           string_builder_append(&builder, i18n_text(STR_SETTINGS_SHUFFLE)); // "Shuffle"
       } else if (frame->status->shuffle == SHUFFLE_ALBUMS) {
           string_builder_append(&builder, i18n_text(STR_MENU_SHUFFLE_ALBUMS));
       }
       if (frame->status->shuffle != SHUFFLE_OFF && frame->status->repeat != REPEAT_OFF) {
           string_builder_append(&builder, " · ");
       }
       if (frame->status->repeat == REPEAT_ONE) {
           string_builder_append(&builder, i18n_text(STR_MENU_REPEAT_ONE));
       } else if (frame->status->repeat == REPEAT_ALL) {
           string_builder_append(&builder, i18n_text(STR_MENU_REPEAT_ALL));
       }
   }
   ```
3. **Blank out "Unknown Artist"**:
   ```c
   ECHO_NP_FACES_CODE static void draw_minimal_artist(Canvas *canvas, TextStyle style, Rect area, const char *artist)
   {
       static const char UNKNOWN_ARTIST[] = "Unknown Artist";
       if (strcmp(artist, UNKNOWN_ARTIST) != 0) {
           label_draw(canvas, style, (Label){.area = area, .text = artist});
       }
   }
   ```
4. **Orientation Switch in `face_minimal_draw()`**:
   - Determine `bool portrait = frame->area.height > frame->area.width;`
   - Calculate cover bounds:
     - Landscape: `(Quad){0, 170, 0, 170, 0, 170}`
     - Portrait: `(Quad){0, 170, 0, 170, 0, 170}`
   - Render text column:
     - Landscape: $x \in [178, 312]$
     - Portrait: $x \in [8, 162]$, below cover ($y \ge 176$)
   - Draw bottom progress bar full width:
     - Landscape: `(Rect){0, 167, 320, 3}`
     - Portrait: `(Rect){0, 317, 170, 3}`

---

### Step 3: Register in `src/np_faces/faces.h`, `np_faces.c`, and CMake
1. In `src/np_faces/faces.h`:
   ```c
   void face_minimal_draw(const NowPlayingFrame *frame, Canvas *canvas);
   ```
2. In `src/np_faces/np_faces.c`:
   - Add chrome color definition for Minimal:
     ```c
     ECHO_NP_FACES_DATA static const ThemeColors MINIMAL_CHROME =
         CHROME(COLOR_RGB(46, 47, 54), COLOR_RGB(22, 23, 27), COLOR_RGB(48, 49, 56),
                COLOR_RGB(242, 242, 245), COLOR_RGB(98, 98, 106), COLOR_RGB(0, 0, 0));
     ```
   - Add `[NP_FACE_MINIMAL] = &MINIMAL_CHROME` to `.chrome`.
   - In `draw()` switch:
     ```c
     case NP_FACE_MINIMAL:
         face_minimal_draw(frame, canvas);
         break;
     ```
3. In `src/np_faces/CMakeLists.txt`:
   - Add `face_minimal.c` to `echo_np_faces` sources.

---

### Step 4: Header Suppression in `now_playing_screen.c`
In `src/app/screens/now_playing_screen.c`:
- In `draw_face()`:
  ```c
  if (face_of(now_playing) != NP_FACE_MINIMAL) {
      header_draw(canvas, &dressed, now_playing->screen.title);
  }
  ```
  *(For Minimal, the header is suppressed so the cover and layout can use the entire screen height, while the status bar still overlays at the top edge transparently).*

---

### Step 5: Verification & Automated Tests
1. **NP Faces Section Compliance**:
   - Run `python3 tools/check_np_faces_sections.py .` $\to$ must return 0 violations.
2. **Settings UI Tests**:
   - In `tests/app/test_settings_screens.c`:
     - Update `NAMES[NP_FACE_COUNT]` with `"Minimal"`.
     - Run `ctest --test-dir build/dev -R test_settings_screens`.
3. **App Tests**:
   - Run `ctest --test-dir build/dev -R test_app`.
4. **Simulator Validation**:
   - **Landscape**:
     - On Dark Theme: Background is pure black `#000000`.
     - Left cover art is full $170 \times 170$ square.
     - Title word-wraps across 2 lines; "Unknown Artist" is blank; modes display "Shuffle".
     - Modes and queue position are left-aligned in the right column.
     - Bottom row shows `elapsed / duration` on the left, with the codec badge right-aligned and raised $1\text{ px}$, and the bitrate right-aligned directly above the codec.
   - **Portrait**:
     - Cover art occupies the top $170 \times 170$ square.
     - Metadata cleanly arranged beneath cover art ($y = 176$ to $314$).
     - Modes and queue position are left-aligned at $x = 8$, matching landscape.
     - Bottom row keeps the same left-aligned times and right-aligned codec/bitrate arrangement as landscape.
     - Progress bar spans the full $170\text{ px}$ width at the very bottom edge ($y = 317$).

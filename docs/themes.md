# Making and sharing themes

A theme file changes EchoPod's colours, the shape of its bars and boxes, and a few spacings. This page covers using one, making one and sharing it. How the firmware reads the file is in `docs/architecture.md` under "Theme files".

## Using a theme

1. Make a folder called `Themes` at the top of the player's microSD card.
2. Copy the `.echotheme` file into it.
3. On the player, open Settings → Display → Theme. Theme files are listed after the built-in themes, each under its file name.

- The player lists up to 8 theme files. A file name (without `.echotheme`) may be 1 to 23 bytes long; longer ones are skipped.
- The name in the list is the file's name, so rename the file to rename the theme.
- While a theme file is worn, Accent color and Highlight are hidden: the file decides those.
- If the file is deleted or damaged, the player goes back to the built-in theme you used last.
- Cover Flow's backdrop, the cover panel on Home and the Now Playing faces (Cassette, VU meters and so on) keep their own colours.

## What a theme can change

- **16 colours:** the background, three strengths of text, the selected row, the header, dividers, the scrollbar, the battery colours and the row arrows.
- **10 boxes:** the selected row (three variants), the header, the toast, the fast-scroll letter, the scrollbar thumb and the three parts of the Now Playing progress bar. Each has a fill (flat or a top-to-bottom gradient), an optional border on any of its edges, an optional shine line, rounded corners and an inset.
- **5 spacings:** row height, padding, scrollbar width and inset, and the gap between a row's label and its value.

- **17 icons:** the status bar's play, pause, Bluetooth and charging marks, the battery, the arrow at the end of a row and Now Playing's play, pause and skip badges.

- **A wallpaper:** a picture behind the lists, one for each way the screen can be held.

- **Fonts:** its own typeface for titles, list text and small text.

- **The Now Playing layout:** where the cover, text, progress bar and times sit, for each way the player is held.

Other screens' layouts can't be changed by a theme yet.

## Making a theme in the editor

The editor is a web page that runs the player's own interface, so what it shows is what the player draws.

- Pick a starting point under **Start from…**, or **Open theme…** to load an `.echotheme` file.
- Change colours, spacing and shapes on the right. The screen on the left updates as you go; the buttons under it jump to Home, a list, Now Playing, a toast, the fast-scroll letter and Settings, in landscape or portrait. Click the screen to drive it with the arrow keys, Enter and Backspace.
- A yellow note appears when something would be hard to read (see Readability below).
- **Download theme** saves the `.echotheme` file for the player. **Download source** saves the same theme as a `.toml` text file, which is what you share in this repository.

The gallery page next to it lists the built-in samples and every theme in this repository's `themes` folder, with a picture of each, a download and an "Open in the editor" link.

### Building the editor yourself

It needs clang with the `wasm32` target and LLVM's `wasm-ld`, `llvm-ar` and `llvm-ranlib` (on Debian and Ubuntu, the `clang`, `lld` and `llvm` packages), Python with `tools/requirements.txt`, and Node 22 for the pictures.

```sh
cmake --preset web
cmake --build --preset web
python tools/web_editor.py --pages web/editor --module build/web/platform/web/echo_preview.wasm --themes assets/themes --community themes --definitions src/ui/include/ui --glyphs assets/fonts/glyph_pack.json --packs src/fonts/packs --output build/web-editor
node web/editor/render_theme.mjs build/web-editor
python -m http.server 8770 --bind 127.0.0.1 --directory build/web-editor
```

Then open `http://127.0.0.1:8770`. The `render_theme.mjs` step draws `previews/<file>.png` for the gallery; given a theme and an output as well (`node web/editor/render_theme.mjs build/web-editor my.echotheme my.png`) it draws just that one.

## Writing a theme by hand

A source is a TOML file. Anything it leaves out comes from `assets/themes/dark.toml`, so a theme can be as small as a name and a few colours:

```toml
name = "Rosewood"
author = "Your name"
description = "Deep burgundy with a rounded selection"
selection = "selection"

[spacing]
row_height = 26

[colors]
background = "#1c0f14"
selection_top = "#a83250"
selection_background = "#7a2338"

[boxes.selection]
fill = ["selection_top", "selection_background"]
ink = "selection_text"
inset = [4, 2]
radius = 11
```

Pack it into a file for the player:

```sh
python tools/theme_pack.py --themes my-themes --base assets/themes/dark.toml --definitions src/ui/include/ui --output my-themes/packed
```

This packs every `.toml` in `my-themes` to `my-themes/packed/<file>.echotheme` and prints a warning for anything hard to read. A packed theme is 195 bytes without icons and about 2,000 with all of them (the player takes up to 3,072), plus about 27,000 for each full-screen wallpaper and about 5,000 for each font.

### Top level

| Entry | Meaning |
| --- | --- |
| `name` | The theme's name, 1 to 23 bytes. Required. |
| `author` | Who made it, one line of at most 40 characters. Shown in the gallery. |
| `description` | One line of at most 120 characters. Shown in the gallery. |
| `selection` | Which box draws the selected row: `selection`, `selection_flat`, `selection_outline` or any other box name. |

### `[colors]`

Colours are written `"#rrggbb"`. The screen shows 65,536 colours, so the last bits of each channel are dropped.

| Name | Used for |
| --- | --- |
| `background` | Behind lists and every plain screen. |
| `text` | Row labels and titles. |
| `text_secondary` | Row values, artists, and other second-rank text. |
| `text_tertiary` | The faintest text and marks, such as Now Playing's small print and the equalizer's scale. |
| `selection_background`, `selection_top`, `selection_shine`, `selection_text` | The selected row: by default its gradient runs from `selection_top` down to `selection_background` with a `selection_shine` line on top and `selection_text` on it. |
| `header_background`, `header_top`, `header_text` | The header bar: its gradient and its title. |
| `divider` | Thin lines, and the track of the progress bar. |
| `scrollbar_thumb` | The scrollbar. |
| `danger` | A low battery. |
| `success` | Charging. |
| `chevron` | The arrow at the end of a row that opens something. |

The names are only defaults for the boxes: a box can use any colour for any of its parts.

### `[boxes.<name>]`

| Box | Draws |
| --- | --- |
| `selection` | The selected row. In the base theme, a gradient bar. |
| `selection_flat` | A second selected row to choose from. In the base theme, a flat bar. |
| `selection_outline` | A third. In the base theme, an outline. |
| `header` | The header bar. |
| `toast` | The short message that pops up at the bottom ("Added to Favorites"). |
| `section` | The big letter shown while fast-scrolling. |
| `scrollbar_thumb` | The moving part of the scrollbar. |
| `progress_track` | The empty part of Now Playing's progress bar. |
| `progress_fill` | The played part. |
| `progress_active` | The played part while you seek or change the volume. |

Only the box the top-level `selection` names draws the selected row; the other two selection boxes aren't drawn. A box you list replaces the base theme's box whole. What you leave out of it is off (no border, no shine, square corners), and a colour you leave out is `background`, so give at least `fill`, and `ink` for a box with text on it.

| Entry | Meaning |
| --- | --- |
| `fill` | A colour name, or `[top, bottom]` for a gradient. |
| `ink` | The colour of the text or arrow drawn on the box. |
| `border`, `border_width` | The border's colour name and its width in pixels. |
| `border_edges` | `"all"`, or a list of `"top"`, `"bottom"`, `"left"`, `"right"`. All four when a width is given and this is left out. |
| `shine`, `shine_height` | A highlight line along the top: its colour name and height in pixels. |
| `inset` | `[across, down]`: how far the box sits inside the area it's given, in pixels. |
| `radius` | Corner radius in pixels, up to 32. It is cut to half the box's shorter side, so a large value makes a pill. |

Colours in a box are names from `[colors]`, never `#rrggbb`.

### `[spacing]`

| Name | Range | Meaning |
| --- | --- | --- |
| `row_height` | 18 to 40 | The height of a list row. |
| `padding` | 4 to 20 | The space at the left and right edges of lists, the header and most screens. |
| `scrollbar_width` | 1 to 8 | The scrollbar's width. |
| `scrollbar_inset` | 0 to 6 | The scrollbar's distance from the edge. |
| `detail_gap` | 2 to 16 | The space between a row's label and its value. |

Leave one out to keep the player's own value.

### `[now_playing]` and `[now_playing_portrait]`

A theme can move, resize, align or hide what the standard Now Playing screen shows, with one layout for the screen held sideways (`[now_playing]`) and one for upright (`[now_playing_portrait]`). Each place is a table of its own:

```toml
[now_playing.cover]
x = 124
y = 30
size = 72

[now_playing.title]
x = 10
y = 104
width = 300
align = "center"

[now_playing.album]
hide = true
```

| Place | What it is | Height if left out |
| --- | --- | --- |
| `cover` | The album cover; give `size` (1 to 170) in place of `width` and `height` | |
| `title`, `artist`, `album` | The three text lines | 22 |
| `format` | The codec box and the bit depth and sample rate | 16 |
| `detail` | The bit rate and the track's place, such as "3 of 12" | 16 |
| `modes` | Shuffle and repeat, and the hint while seeking | 16 |
| `progress` | The progress bar, which also shows the volume while it changes | 3 |
| `times` | The time played at the left end and the time left at the right end | 16 |
| `lyrics` | Where lyrics show; as many lines as its height holds, at most five | 95 |

- `x` and `y` are pixels from the top-left corner of the screen (the header is at the top, 24 pixels high sideways and 40 upright). The screen is 320 x 170 sideways and 170 x 320 upright.
- `align` is `"left"`, `"center"` or `"right"`, and moves text that is shorter than its box. Text longer than its box scrolls.
- A place the theme leaves out stays where the player puts it, so a theme can move only the progress bar. The player's places depend on the screen: without a cover the text moves left (sideways) or up (upright).
- While lyrics show they hide the cover and any text line they overlap, except the title. Put `lyrics` where the cover is to show lyrics in its place, or where the text is to show them in place of the text.
- The faces (Cassette, VU meters, Turntable, LCD deck) have their own layouts.

In the editor, show Now Playing, open **Now Playing layout → Places**, and drag a box on the screen, or tick a place and type its numbers. **Start from the player's layout** fills every place with where the player puts it now.

### `[fonts.title]`, `[fonts.body]` and `[fonts.small]`

A theme can bring its own fonts for three kinds of text:

| Font | Used for | Tallest line |
| --- | --- | --- |
| `title` | Header titles | 20 px |
| `body` | Lists, menus and most text, and the large status bar | 20 px |
| `small` | Second lines, small print and the status bar | 15 px |

The easy way is to name a TTF or OTF file that sits beside the theme's source:

```toml
[fonts.body]
file = "MyFont-Medium.ttf"

[fonts.small]
file = "MyFont-Regular.ttf"
size = 11
```

- Without `size`, the packer picks the largest pixel size whose line fits. A size whose line is too tall is refused.
- The font is drawn for the basic Latin letters, digits and punctuation, plus a few marks (curly quotes, dashes, the ellipsis, the euro sign, ★ and ✓) when the font has them. Any character a theme font lacks, such as accented letters, Cyrillic, Chinese, Japanese and Korean, is drawn in the player's own font on the same line.
- The editor's **Download source** writes a font out as its glyphs (each `{char, left, top, advance, rows}`, the rows like an icon's), so a source from the editor needs no font file.
- Only share a font whose licence allows it to be passed on, such as the SIL Open Font License. Put the licence in the pull request.

Fonts and the wallpaper share 28,224 bytes of the player's memory. A 16 px font takes about 5 KB and a whole-screen wallpaper 28 KB, so a theme with fonts needs a repeating or smaller wallpaper. The packer says how much a theme needs when it is too much, and the editor's status line shows how much of the memory a theme uses.

### `[wallpaper]` and `[wallpaper_portrait]`

A wallpaper is a picture painted behind the lists, in place of the background colour. `[wallpaper]` is for the landscape screen, 320 x 170, and `[wallpaper_portrait]` for the upright one, 170 x 320. The player shows 16 colours of it, mixed in a fine dot pattern to suggest more.

The easy way is to name a picture that sits beside the theme's source:

```toml
[wallpaper]
image = "rosewood.png"
dim = 40

[wallpaper_portrait]
image = "rosewood-tall.png"
dim = 40
```

- A picture bigger than the screen is scaled to cover it and cut at the middle. A smaller one keeps its size and repeats across and down, so a small pattern makes a small file.
- `dim` fades the picture toward the theme's `background` colour: 0 leaves it alone, 90 leaves a hint. Text sits straight on the wallpaper, so most pictures need 30 to 60.
- With only `[wallpaper]`, the upright screen stays plain, unless the wallpaper is at most 170 x 170: then it is used on both screens.
- Fonts and wallpaper share 28,224 bytes of the player's memory; see Fonts above.
- A full-screen wallpaper adds about 27 KB to the theme file for each screen.

A wallpaper can also be written out, which is what the editor's **Download source** does: `palette` lists up to 16 colours and each row is one character per pixel, the colour's place in the palette as `0`-`9` then `a`-`f`. Rows need the same, even length.

```toml
[wallpaper]
palette = ["#eef4f8", "#dfe9f1"]
rows = [
    "00000000",
    "00000000",
    "00000000",
    "00010000",
    "00000000",
    "00000000",
    "00000000",
    "00000000",
]
```

Where it shows: lists, menus, Settings, messages and the standard Now Playing screen. Cover Flow, the Now Playing faces, the cover panel on Home and the charging screen keep their own backgrounds. After the player has been connected to a computer or has rescanned the library, the wallpaper is read from the card again, so the theme file must stay in `Themes`.

In the editor, the Wallpaper section takes an image file for each screen, with a **Fade** slider and a **Repeat a small picture** box, and shows the 16-colour result.

### `[icons]`

An icon is a small shape, not a picture: you say how solid each pixel is and the player colours it, so the icon follows the theme (and turns red on a low battery, dim when Bluetooth has nothing connected, and so on). Leave an icon out to keep the player's own.

Each icon is a list of rows of text, one character per pixel: `.` is clear, `f` is solid, and `1`-`9`, `a`-`e` are the steps between, for soft edges.

```toml
[icons]
chevron = [
    ".4....",
    ".d5...",
    "..a8..",
    "...7b.",
    "....d5",
    "...6c.",
    "..98..",
    ".d5...",
    ".4....",
]
```

| Name | Largest size | Where | Coloured with |
| --- | --- | --- | --- |
| `status_play`, `status_pause` | 10 x 10 | Left of the status bar | The header's text |
| `status_bluetooth` | 7 x 10 | After the play mark | The header's text; `text_tertiary` when nothing is connected |
| `status_bolt` | 6 x 10 | Left of the battery while charging | `success` |
| `battery` | 22 x 10 | Right of the status bar: the frame | The header's text |
| `battery_level` | 22 x 10 | Drawn over the frame: the charge | The header's text; `danger` when low; `success` while charging |
| `chevron` | 12 x 18 | End of a row that opens something | `chevron`; the selected row's ink |
| `badge_play`, `badge_pause`, `badge_next`, `badge_previous` | 18 x 18 | Over the cover on Now Playing when you press a key | `text` on a `background` square |
| `status_play_large`, `status_pause_large` | 14 x 14 | The same, on the upright screen | |
| `status_bluetooth_large` | 9 x 14 | | |
| `status_bolt_large` | 8 x 14 | | |
| `battery_large`, `battery_level_large` | 30 x 14 | | |

- An icon may be smaller than its largest size. It is drawn centred in its place; the chevron's row makes room for its width.
- Every row of an icon needs the same length.
- `battery` and `battery_level` come together and at the same size. Draw the level as the battery looks when full: the player shows it from the left as far as the charge goes, so a level made of separate blocks works too.
- The upright screen has taller status icons. With no large icon the small one is used there, centred.
- The Now Playing faces (Cassette, VU meters and so on) keep their own icons.

In the editor, the Icons section has a grid for each one: drag to paint, Shift-drag or right-drag to erase, set how solid the brush is, or fill the grid from an image file (its transparency becomes the shape; a picture without transparency is read as dark ink on light paper).

## Readability

The packer and the editor measure the contrast of the pairs below and warn when one is under its floor. Contrast runs from 1 (the same colour) to 21 (black on white).

| What | Floor |
| --- | --- |
| `text` on `background` | 4.5 |
| `text_secondary` on `background` | 3.0 |
| `text_tertiary` on `background` | 2.5 |
| `chevron` on `background` | 2.0 |
| `danger` on `background`, `success` on `background` | 3.0 |
| The selected row's `ink` on its fill | 3.0 |
| The header's `ink` on its fill | 4.5 |
| The toast's `ink` on its fill | 4.5 |
| The fast-scroll letter's `ink` on its fill | 4.5 |
| The progress bar's fill on its track | 2.0 |
| `text` over the wallpaper, leaving out the worst tenth of its pixels | 3.0 |

For a gradient, the ink is measured against the end it reads worse on. A warning doesn't stop a theme from packing or from working on the player; `--strict` turns warnings into a failure, and themes in this repository's `themes` folder must pass it.

## Sharing a theme

**As a file.** The `.echotheme` file is the whole theme. Send it or post it anywhere; whoever gets it copies it into `Themes` on their card. Share the `.toml` as well if you'd like others to build on it: the editor's **Download source** writes it.

**In the gallery.** Open a pull request that adds one file, `themes/<file>.toml`:

- Name the file with 1 to 23 of `a-z`, `0-9`, `-` and `_`, and not `dark` or `ember`.
- Give it a `name`, an `author` and a `description`.
- A picture the theme names for its wallpaper goes in `themes` beside it, as a PNG, and a font file as a TTF or OTF with its licence.
- It must pack and pass the readability checks:

  ```sh
  python tools/theme_pack.py --themes themes --base assets/themes/dark.toml --definitions src/ui/include/ui --output build/themes --strict
  ```

The `themes` workflow runs the same check on the pull request, builds the editor and attaches a picture of every theme to the run (the `theme-previews` artifact). When the pull request is merged the gallery is rebuilt with the new theme in it.

# Making and sharing themes

A theme file changes EchoPod's colours, the shape of its bars and boxes, and a few spacings. This page covers using one, making one and sharing it. How the firmware reads the file is in `docs/architecture.md` under "Theme files".

## Using a theme

1. Make a folder called `Themes` at the top of the player's microSD card.
2. Copy the `.echotheme` file into it.
3. On the player, open Settings → Display → Theme. Theme files are listed after the built-in themes, each under its file name.

- The player lists up to 8 theme files. A file name (without `.echotheme`) may be 1 to 23 bytes long; longer ones are skipped.
- The name in the list is the file's name, so rename the file to rename the theme.
- Accent color lists the accents the theme offers, and is hidden for a theme that offers none. Dark and Light offer eight; a theme file can offer up to eight of its own.
- Highlight (gradient or flat) shows for any theme whose selected row is drawn as a gradient.
- If the file is deleted or damaged, the player goes back to the built-in theme you used last.
- Cover Flow is drawn in the theme's colours, unless the theme asks for the dark look as Light, Paper and Olive do.
- The cover panel on Home and the Now Playing faces (Cassette, VU meters and so on) keep their own colours.

The built-in themes are theme files too, kept inside the firmware: their sources are in `assets/themes`, and the editor's **Start from** list opens any of them.

## What a theme can change

- **16 colours:** the background, three strengths of text, the selected row, the header, dividers, the scrollbar, the battery colours and the row arrows.
- **10 boxes:** the selected row (three variants), the header, the toast, the fast-scroll letter, the scrollbar thumb and the three parts of the Now Playing progress bar. Each has a fill (flat or a top-to-bottom gradient), an optional border on any of its edges, an optional shine line, rounded corners and an inset.
- **5 spacings:** row height, padding, scrollbar width and inset, and the gap between a row's label and its value.

- **17 icons:** the status bar's play, pause, Bluetooth and charging marks, the battery, the arrow at the end of a row and Now Playing's play, pause and skip badges.

- **A wallpaper:** a picture behind the lists, one for each way the screen can be held.

- **Fonts:** its own typeface for titles, list text and small text.

- **The Now Playing layout:** where the cover, text, progress bar and times sit, for each way the player is held.

- **A Now Playing scene:** its own drawing of the whole Now Playing screen, from a short script.

- **A Home scene:** its own Home screen: the menu as a list, tiles or one item at a time, the album art, what's playing, anywhere on the screen. The buttons still move through the menu from top to bottom.

The other screens are the theme's lists, drawn in its colours, boxes, spacing, wallpaper, fonts and icons.

## Making a theme in the editor

The editor is a web page that runs the player's own interface, so the screen in the middle is exactly what the player draws. Nothing to install and no code to write.

- **Steps** down the left take you through it: Start, Colours, Lists & shapes, Home, Now Playing, Wallpaper, Fonts, Icons, and Save & share. You can visit them in any order.
- **Click anything on the screen** to jump to what colours it. The tabs above the screen switch between Home, the lists, Now Playing, Settings and a message, and the two buttons beside them turn the player sideways or upright. The buttons under the screen (or the arrow keys, Enter and Backspace once you click the screen) drive it.
- **Start** begins from any built-in or shared theme, or opens an `.echotheme` file.
- **Now Playing** is yours to arrange. Choose "My own design" and the screen gets layers you can drag and resize on the preview: the album cover (square or round, with a frame), text (song, artist, album, lyrics, the audio format, your own words), the times, a progress bar or a ring of lights, shapes, the play and pause mark, and a turning mark that spins while a song plays. Each takes the theme's colours or your own. The sideways and upright screens are arranged separately; "Copy the … arrangement" starts one from the other.
- **Home** is arranged the same way. Its layers are the menu (a list drawn like the theme's other lists, tiles, or one item at a time with the ones before and after it), the album cover (with a reflection if you like), text (the highlighted menu item, the song, artist or album playing, or your own words), the progress bar, the times, shapes and the play and pause mark. The background can be the wallpaper, one colour or a gradient.
- A note appears when something would be hard to read (see Readability below). Undo and redo are at the top, and your work is kept in the browser until you start over.
- **Download for my player** saves the `.echotheme` file. **Download source** saves the same theme as a `.toml` text file, which is what you share in this repository.

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

This packs every `.toml` in `my-themes` to `my-themes/packed/<file>.echotheme` and prints a warning for anything hard to read. A packed theme is 205 bytes without icons and about 2,000 with all of them (the player takes up to 3,072), plus about 27,000 for each full-screen wallpaper and about 5,000 for each font.

### Top level

| Entry | Meaning |
| --- | --- |
| `name` | The theme's name, 1 to 23 bytes. Required. |
| `author` | Who made it, one line of at most 40 characters. Shown in the gallery. |
| `description` | One line of at most 120 characters. Shown in the gallery. |
| `selection` | Which box draws the selected row: `selection`, `selection_flat`, `selection_outline` or any other box name. |
| `cover_flow` | `"theme"` (the default) draws Cover Flow in the theme's colours; `"dark"` draws it in the player's dark look, which suits light themes. |
| `home_art` | `true` (the default) shows album art beside the Home menu, or below it when the player is upright; `false` gives the menu the whole screen. |

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

### `[[accents]]`

A theme can offer up to eight accents. Each replaces the selected row's three colours, and the player lists them under Settings → Display → Accent color:

```toml
[[accents]]
name = "Red"
selection_background = "#c0262e"
selection_top = "#e65458"
selection_shine = "#f48080"

[[accents]]
name = "Ruby"
selection_background = "#900020"
selection_top = "#b00030"
selection_shine = "#e06080"
```

- `name` is 1 to 12 bytes. Blue, Red, Orange, Green, Teal, Purple, Pink and Graphite are shown in the player's language; any other name is shown as written.
- `selection_background` fills the row (and the whole row with the flat Highlight), `selection_top` is the top of the gradient and `selection_shine` the shine line.
- The first accent is the one a player uses until someone picks another. A theme without accents keeps its own selection colours.
- Accents are never taken from `dark.toml`: a theme offers only the ones it lists.

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
| `opacity` | How solid the box is, from 0 to 100 percent. 100, the default, is fully solid; lower values let the wallpaper or whatever is underneath show through. The player has 16 steps, so the value is rounded to the nearest one. |

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

### `[now_playing_scene]`

A scene draws the whole Now Playing screen, below the header, from a short script: shapes, text, the cover and the progress, moving with the music. The built-in Cassette, VU meters, Turntable and LCD deck themes are made this way; their scripts are in `assets/themes`.

```toml
[now_playing_scene]
file = "orbit.scene"

[now_playing_scene.chrome]
header_top = "#1b2140"
header_background = "#10142a"
header_text = "#eef1fa"
```

- `file` names a script beside the source (or `script` holds it in the source, as a multi-line string).
- `chrome` is optional: the colours the header and status bar take while the scene shows. Any colour it leaves out is the theme's own.
- On the player, wearing the theme shows its scene on Now Playing; a theme without one shows the standard Now Playing in its colours.
- A scene takes at most 6,144 bytes once packed, and a theme with a Home scene too shares those 6,144 bytes between them. The packer and the editor say when it's too big, and where a script has a mistake.

### `[home_scene]`

The same for the Home screen, with the same keys: `file` or `script`, and an optional `chrome`.

```toml
[home_scene]
file = "my-home.scene"
```

A Home scene draws everything below the title bar, so it decides where the menu goes and what sits beside it. The player still owns the menu itself: the buttons move the highlight from one item to the next in order, and Play opens it. A theme without a Home scene shows the standard Home: the list, with the album art beside it unless `home_art = false`.

## Writing a scene

`themes/orbit.scene` is a complete example in 40 lines; `assets/themes/cassette.scene` is a longer one. A script runs from top to bottom every time the screen is drawn, about ten times a second while a track plays.

### The language

```
const DEEP = #181418
let wide = width > height
let cx = wide ? 84 : width / 2
rect(x, y, width, height, DEEP)
if playing {
    spoke(cx, y + 80, (tick * 3) & 255, 24, 60, 66, #c8eeff)
} else if paused {
    icon(pause, cx - 9, y + 71, 18, 18, #c8eeff)
}
repeat mark from 0 to 48 {
    spoke(cx, y + 80, mark * 16 / 3, 40, 60, 68, mark < progress(48) ? #7fd3ff : #232a42)
}
```

- **Numbers** are whole, from about −2 billion to 2 billion, and work as in C: `/` drops the remainder and `%` keeps the sign of the left side. `0x` starts a hex number, and `#rrggbb` is a colour.
- **Names:** `const NAME = ...` must be known before playing; `let name = ...` is worked out as the scene runs, and `name = ...` changes it later. A name made inside `{ }` ends with the block, so both sides of an `if` can make their own.
- **Operators,** from loosest to tightest: `? :`, `||`, `&&`, `|`, `^`, `&`, `== !=`, `< <= > >=`, `<< >> >>>` (`>>>` shifts in zeros), `+ -`, `* / %`, and `-` and `!` in front. A comparison gives 1 or 0.
- **`if`** with `else if` and `else`, and **`repeat name from a to b { }`**, which runs with `name` from `a` up to `b − 1`.
- Words in `"quotes"` are text to show. Statements need no separators.

### What a scene knows

| Name | Meaning |
| --- | --- |
| `x`, `y`, `width`, `height` | The area below the header: 320 × 146 from y 24 held sideways, 170 wide upright |
| `playing`, `paused`, `scrubbing`, `volume_shown` | 1 when true |
| `tick` | Tenths of a second into the track; it moves only while playing |
| `now` | Milliseconds since the player started |
| `shown`, `duration` | Where the track is (the seek target while seeking) and how long it is, in milliseconds |
| `volume` | 0 to 32 |
| `badge` | The skip, play or pause mark to show: `none`, `play`, `pause`, `next` or `previous` |
| `shuffle_mode`, `repeat_mode` | 0 when off, then the player's shuffle and repeat modes in Settings order |
| `track` | The track's place in the queue |
| `has_cover` | 1 when the track has a cover; on Home, when there is album art to show |

### Functions

| Function | Gives |
| --- | --- |
| `min(a, b)`, `max(a, b)`, `abs(a)` | The smaller, the larger, the size |
| `sin(angle)`, `cos(angle)` | −255 to 255, for an angle of 0 to 255 per turn |
| `isqrt(a)` | The whole square root |
| `progress(w)` | How much of `w` the track has played, 0 to `w` |
| `volume_width(w)` | How much of `w` the volume fills |
| `present(text)` | 1 when that text is there, such as `present(lyric)` |
| `width(text, font)` | How wide that text is, in pixels |
| `segments(digit)` | The seven-segment pattern of a digit, for `digit()` |
| `items()` | On Home, how many menu items there are; 0 on Now Playing |
| `selected()` | On Home, which item is highlighted, from 0 |
| `item(n)` | Menu item `n`, to show with `text()` or `line()`: `line(item(selected()), title, ...)` |

### Drawing

Places and sizes are pixels on the screen. `ring` takes sixteenths of a pixel for its smooth edges (`x * 16 + 8` is the middle of pixel `x`), and so does the width of a `spoke`.

| Command | Draws |
| --- | --- |
| `rect(x, y, w, h, colour)` | A filled box |
| `rounded(x, y, w, h, radius, colour)` | A box with rounded corners |
| `ring(x16, y16, outer16, inner16, colour)` | A disc, or a ring when `inner16` is above 0 |
| `spoke(x, y, angle, width16, from, to, colour)` | A bar turned to `angle` around (`x`, `y`), from `from` to `to` pixels out |
| `trapezoid(x, y, w, h, inset, colour)` | A box narrowing by `inset` on each side toward the bottom |
| `gradient(x, y, w, h, top, bottom)` | A box shading from `top` to `bottom` |
| `lattice(x, y, w, h, pitch, colour)` | Grid lines every `pitch` pixels |
| `segment_style(w, h, thickness, lean, lit, unlit)`, `digit(x, y, segments)`, `colon(x, y, lit)` | Seven-segment digits, as on an LCD |
| `clip(x, y, w, h)`, `unclip()` | Draw only inside a box until `unclip()`, up to 4 deep |
| `cover(x, y, size)` | The cover, square |
| `round_cover(x, y, radius)` | The cover cut to a circle around (`x`, `y`) |
| `icon(kind, x, y, w, h, colour)` | `play`, `pause`, `next` or `previous` |
| `bar(x, y, w, h, track, fill, hot)` | The progress bar; it shows the volume while that changes, in `hot` |
| `outline(x, y, w, h, colour)` | A one-pixel frame |
| `backdrop(x, y, w, h)` | The theme's wallpaper, or its background colour when it has none |
| `reflect(x, y, size, rows)` | A fading reflection of the cover drawn at (`x`, `y`), `rows` tall, below it |
| `menu(x, y, w, h)` | On Home, the menu as a list, in the theme's rows, selected row and arrows |

On Home, the cover is the album art Home shows: the album playing, or a different album every few seconds. Song, artist and album are the track playing, and empty when nothing is.

Text takes what to show: `song`, `artist`, `album`, `lyric`, `codec`, `stream` (such as "24-bit · 96 kHz"), `detail` (the bit rate and the track's place), `modes`, `feedback` (the seek hint or "Volume 20"), `elapsed`, `remaining`, or your own words in quotes. It also takes a font (`title`, `body` or `small`) and, where it says `align`, one of `left`, `center` or `right`:

| Command | Draws |
| --- | --- |
| `text(what, font, x, y, w, h, align, colour)` | The text, cut to fit the box |
| `line(what, font, x, y, w, h, align, colour)` | The text, scrolling while playing when it is wider than the box |
| `plain(what, font, x, y, w, h, colour)` | The text from the box's left edge |
| `times(font, x, y, w, h, left, right)` | The time played at the left of the box and the time left at the right |
| `format(font, x, y, w, h, align, colour)` | The codec in a box, then the stream |
| `codec(font, x, y, w, h, colour)` | The codec in a box |

A scene can't run away: it stops after 20,000 steps, a mistake in a file stops only that scene, and a scene the player can't read leaves Now Playing as Standard.

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

- Name the file with 1 to 23 of `a-z`, `0-9`, `-` and `_`, and not the name of a file in `assets/themes`.
- Give it a `name`, an `author` and a `description`.
- A picture the theme names for its wallpaper goes in `themes` beside it, as a PNG, a font file as a TTF or OTF with its licence, and a scene script as `<file>.scene`.
- It must pack and pass the readability checks:

  ```sh
  python tools/theme_pack.py --themes themes --base assets/themes/dark.toml --definitions src/ui/include/ui --output build/themes --strict
  ```

The `themes` workflow runs the same check on the pull request, builds the editor and attaches a picture of every theme to the run (the `theme-previews` artifact). When the pull request is merged the gallery is rebuilt with the new theme in it.

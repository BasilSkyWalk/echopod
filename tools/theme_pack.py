import argparse
import json
import re
import struct
import sys
import tomllib
from pathlib import Path

import scene_compile

MAGIC = b"ETHM"
VERSION = 1
HEADER_SIZE = 48
CHECKED_FROM = 16
NAME_SIZE = 24
MAX_SIZE = 3072
BOX_SIZE = 12
OPAQUE = 15
MAX_RADIUS = 32
HASH_START = 2166136261
HASH_PRIME = 16777619
EDGES = {"top": 1, "bottom": 2, "left": 4, "right": 8}
ALL_EDGES = 15
BYTE_FIELDS = ("border_width", "shine_height")
BOX_KEYS = {"fill", "border", "shine", "ink", "border_width", "border_edges", "shine_height", "inset",
            "radius", "opacity"}
THEME_KEYS = {"name", "author", "description", "selection", "colors", "boxes", "spacing", "icons",
              "wallpaper", "wallpaper_portrait", "fonts", "now_playing",
              "now_playing_portrait", "accents", "cover_flow", "now_playing_scene",
              "home_art", "home_scene"}
LAYOUTS = ("now_playing", "now_playing_portrait")
LAYOUT_KEYS = {"x", "y", "width", "height", "align", "hide"}
LAYOUT_COVER_KEYS = {"x", "y", "size", "hide"}
LAYOUT_BOX = struct.Struct("<hhHBB")
LAYOUT_REACH = 320
LAYOUT_MOST_COVER = 170
LAYOUT_UNSET = 0xFF
ALIGNS = ("left", "center", "right")
WALLPAPERS = (("wallpaper", 320, 170), ("wallpaper_portrait", 170, 320))
WALLPAPER_KEYS = {"image", "dim", "palette", "rows"}
ASSET_HEADER_SIZE = 56
ACCENT = struct.Struct("<HHHB13s")
ACCENT_KEYS = {"name", "selection_background", "selection_top", "selection_shine"}
ACCENT_NAMES = ("blue", "red", "orange", "green", "teal", "purple", "pink", "graphite")
ACCENT_NAME_SIZE = 13
MOST_ACCENTS = 8
CUSTOM_ACCENT = 0xFF
COVER_FLOW_STYLES = ("theme", "dark")
SCENE_KIND = 5
SCENE_SOURCE_KIND = 6
SCENE_DESIGN_KIND = 7
SCENES = (("now_playing_scene", SCENE_KIND, SCENE_SOURCE_KIND, SCENE_DESIGN_KIND),
          ("home_scene", 8, 9, 10))
SCENE_KEYS = {"script", "file", "chrome", "design"}
SCENE_MOST_BYTES = 6144
SCENE_CHROME = 1
FLAG_DARK_COVER_FLOW = 1
FLAG_NO_HOME_ART = 2
ASSET_ENTRY_SIZE = 16
ASSET_MEMORY = 28224
WALLPAPER_PAIR_BYTES = 1024
FIRST_FONT_KIND = 2
MAX_FONT_GLYPHS = 512
LEAST_FONT_SIZE = 8
MOST_FONT_SIZE = 32
GLYPH_KEYS = {"char", "left", "top", "advance", "rows"}
GLYPH_RECORD = struct.Struct("<HHBBbbBx")
FONT_RANGES = Path(__file__).resolve().parent.parent / "assets" / "fonts" / "fonts.json"
PALETTE_COLORS = 16
HEX_DIGITS = "0123456789abcdef"
ICON_LEVELS = ".123456789abcdef"
ICON_PAIRS = (("battery", "battery_level"), ("battery_large", "battery_level_large"))
CREDIT_LIMITS = {"author": 40, "description": 120}
HEX_COLOR = re.compile(r"^#([0-9a-fA-F]{6})$")
ENTRY = re.compile(r"^THEME_(?:COLOR|BOX)\(\w+,\s*(\w+)\)\s*$")
SPACING_ENTRY = re.compile(r"^THEME_SPACING\(\w+,\s*(\w+),\s*(\d+),\s*(\d+)\)\s*$")
ICON_ENTRY = re.compile(r"^THEME_ICON\(\w+,\s*(\w+),\s*(\d+),\s*(\d+)\)\s*$")
FONT_ENTRY = re.compile(r"^THEME_FONT\(\w+,\s*(\w+),\s*(\d+)\)\s*$")
LAYOUT_ENTRY = re.compile(r"^THEME_LAYOUT\(\w+,\s*(\w+),\s*(\d+)\)\s*$")
BASE_THEME = "dark"
SOURCE_SUFFIX = ".toml"
PACKAGE_SUFFIX = ".echotheme"


class ThemeError(Exception):
    pass


def names_in(definition):
    names = []
    for line in definition.read_text(encoding="utf-8").splitlines():
        match = ENTRY.match(line.strip())
        if match:
            names.append(match.group(1))
    if not names:
        raise ThemeError(f"{definition} lists nothing")
    return names


def spacing_in(definition):
    found = []
    for line in definition.read_text(encoding="utf-8").splitlines():
        match = SPACING_ENTRY.match(line.strip())
        if match:
            found.append((match.group(1), int(match.group(2)), int(match.group(3))))
    return found


def icons_in(definition):
    if not definition.exists():
        return []
    found = []
    for line in definition.read_text(encoding="utf-8").splitlines():
        match = ICON_ENTRY.match(line.strip())
        if match:
            found.append((match.group(1), int(match.group(2)), int(match.group(3))))
    return found


def layout_in(definition):
    if not definition.exists():
        return []
    found = []
    for line in definition.read_text(encoding="utf-8").splitlines():
        match = LAYOUT_ENTRY.match(line.strip())
        if match:
            found.append((match.group(1), int(match.group(2))))
    return found


def fonts_in(definition):
    if not definition.exists():
        return []
    found = []
    for line in definition.read_text(encoding="utf-8").splitlines():
        match = FONT_ENTRY.match(line.strip())
        if match:
            found.append((match.group(1), int(match.group(2))))
    return found


def checksum(data):
    value = HASH_START
    for byte in data:
        value = ((value ^ byte) * HASH_PRIME) & 0xFFFFFFFF
    return value


def rgb565(text, where):
    match = HEX_COLOR.match(text) if isinstance(text, str) else None
    if match is None:
        raise ThemeError(f"{where}: {text!r} is not a colour like #1a2b3c")
    value = int(match.group(1), 16)
    red, green, blue = value >> 16, value >> 8 & 0xFF, value & 0xFF
    return (red & 0xF8) << 8 | (green & 0xFC) << 3 | blue >> 3


def color_id(name, colors, where):
    if name not in colors:
        raise ThemeError(f"{where}: {name!r} is not a colour name")
    return colors.index(name)


def small_number(value, where):
    if not isinstance(value, int) or isinstance(value, bool) or not 0 <= value <= 255:
        raise ThemeError(f"{where}: {value!r} is not a whole number from 0 to 255")
    return value


def edges_of(value, where):
    if value == "all":
        return ALL_EDGES
    if not isinstance(value, list) or any(edge not in EDGES for edge in value):
        raise ThemeError(f'{where}: use "all" or a list of top, bottom, left, right')
    return sum(EDGES[edge] for edge in set(value))


def fill_of(value, colors, where):
    pair = value if isinstance(value, list) else [value, value]
    if len(pair) != 2:
        raise ThemeError(f"{where}: fill is one colour name, or [top, bottom]")
    return [color_id(name, colors, where) for name in pair]


def inset_of(value, where):
    if not isinstance(value, list) or len(value) != 2:
        raise ThemeError(f"{where}: inset is [across, down]")
    return [small_number(part, where) for part in value]


def box_bytes(table, colors, where):
    if not isinstance(table, dict) or set(table) - BOX_KEYS:
        unknown = ", ".join(sorted(set(table) - BOX_KEYS)) if isinstance(table, dict) else "it"
        raise ThemeError(f"{where}: {unknown} is not something a box has")
    top, bottom = fill_of(table.get("fill", colors[0]), colors, where)
    named = [color_id(table.get(key, colors[0]), colors, where) for key in ("border", "shine", "ink")]
    width, shine_height = (small_number(table.get(key, 0), where) for key in BYTE_FIELDS)
    edges = edges_of(table.get("border_edges", "all" if width else []), where)
    across, down = inset_of(table.get("inset", [0, 0]), where)
    radius = small_number(table.get("radius", 0), where)
    if radius > MAX_RADIUS:
        raise ThemeError(f"{where}: a radius may be at most {MAX_RADIUS}")
    opacity = table.get("opacity", 100)
    if isinstance(opacity, bool) or not isinstance(opacity, int) or not 0 <= opacity <= 100:
        raise ThemeError(f"{where}: opacity is a percentage from 0 to 100")
    see_through = OPAQUE - (opacity * OPAQUE + 50) // 100
    return bytes([top, bottom, *named, width, edges, shine_height, across, down, radius, see_through])


def spacing_bytes(table, spacing, where):
    known = {name for name, _, _ in spacing}
    if not isinstance(table, dict) or set(table) - known:
        unknown = ", ".join(sorted(set(table) - known)) if isinstance(table, dict) else "it"
        raise ThemeError(f"{where}: {unknown} is not a spacing name")
    stored = []
    for name, least, most in spacing:
        value = table.get(name)
        if value is not None and not (isinstance(value, int) and not isinstance(value, bool)
                                      and least <= value <= most):
            raise ThemeError(f"{where}: {name} must be a whole number from {least} to {most}")
        stored.append(0 if value is None else value + 1)
    return bytes(stored)


def icon_levels(row, where):
    levels = [ICON_LEVELS.find(mark) if mark != "0" else 0 for mark in row.lower()]
    if -1 in levels:
        raise ThemeError(f"{where}: a row is made of . and 1-9, a-f, from clear to solid")
    return levels + [0] * (len(levels) % 2)


def icon_mask(rows, limit, where):
    if not isinstance(rows, list) or not rows or any(not isinstance(row, str) for row in rows):
        raise ThemeError(f"{where}: an icon is a list of rows of text")
    width = len(rows[0])
    if width == 0 or any(len(row) != width for row in rows):
        raise ThemeError(f"{where}: every row needs the same length")
    if width > limit[0] or len(rows) > limit[1]:
        raise ThemeError(f"{where}: at most {limit[0]} wide and {limit[1]} high")
    levels = [level for row in rows for level in icon_levels(row, where)]
    packed = bytes(first << 4 | second for first, second in zip(levels[::2], levels[1::2]))
    return width, len(rows), packed


def check_icon_pairs(masks, where):
    for frame, level in ICON_PAIRS:
        sizes = [masks[name][:2] if name in masks else None for name in (frame, level)]
        if sizes[0] != sizes[1]:
            raise ThemeError(f"{where}: {frame} and {level} go together, at one size")


def icon_section(table, icons, where):
    known = [name for name, _, _ in icons]
    if not isinstance(table, dict) or set(table) - set(known):
        unknown = ", ".join(sorted(set(table) - set(known))) if isinstance(table, dict) else "it"
        raise ThemeError(f"{where}: {unknown} is not an icon name")
    masks = {name: icon_mask(table[name], (width, height), f"{where}: icon {name}")
             for name, width, height in icons if name in table}
    check_icon_pairs(masks, where)
    count = max((known.index(name) + 1 for name in masks), default=0)
    entries, data = b"", b""
    for name in known[:count]:
        width, height, packed = masks.get(name, (0, 0, b""))
        entries += struct.pack("<BBH", width, height, len(data) if packed else 0)
        data += packed
    return count, entries + data


def name_bytes(name, where):
    encoded = name.encode("utf-8") if isinstance(name, str) else b""
    if not encoded or len(encoded) >= NAME_SIZE:
        raise ThemeError(f"{where}: the name needs 1 to {NAME_SIZE - 1} bytes of UTF-8")
    return encoded.ljust(NAME_SIZE, b"\x00")


def check_keys(theme, where):
    if set(theme) - THEME_KEYS:
        raise ThemeError(f"{where}: unknown entries: " + ", ".join(sorted(set(theme) - THEME_KEYS)))
    for key, most in CREDIT_LIMITS.items():
        value = theme.get(key, "")
        if not isinstance(value, str) or len(value) > most or not value.isprintable():
            raise ThemeError(f"{where}: {key} is one line of at most {most} characters")


def merged(base, theme, key, known, where):
    table = dict(base.get(key, {}))
    for name, value in theme.get(key, {}).items():
        if name not in known:
            raise ThemeError(f"{where}: {name!r} is not one of the {key}")
        table[name] = value
    missing = [name for name in known if name not in table]
    if missing:
        raise ThemeError(f"{where}: no value for {key} " + ", ".join(missing))
    return table


def own_table(theme, key, where):
    table = theme.get(key, {})
    if not isinstance(table, dict):
        raise ThemeError(f"{where}: {key} is a table of names and values")
    return table


def wallpaper_rows(rows, colors_used, where):
    packed = bytearray()
    for row in rows:
        indexes = [HEX_DIGITS.find(mark) for mark in row.lower()]
        if min(indexes) < 0 or max(indexes) >= colors_used:
            raise ThemeError(f"{where}: a row is made of the palette's numbers, 0-9 then a-f")
        packed += bytes(first << 4 | second for first, second in zip(indexes[::2], indexes[1::2]))
    return bytes(packed)


def wallpaper_section(table, limit, where):
    if not isinstance(table, dict) or set(table) - WALLPAPER_KEYS:
        raise ThemeError(f"{where}: a wallpaper has an image, or a palette and rows")
    palette, rows = table.get("palette"), table.get("rows")
    if not isinstance(palette, list) or not 1 <= len(palette) <= PALETTE_COLORS:
        raise ThemeError(f"{where}: the palette is a list of 1 to {PALETTE_COLORS} colours")
    if not isinstance(rows, list) or not rows or any(not isinstance(row, str) for row in rows):
        raise ThemeError(f"{where}: rows is a list of rows of text")
    width = len(rows[0])
    if width < 2 or width % 2 or any(len(row) != width for row in rows):
        raise ThemeError(f"{where}: every row needs the same, even length")
    if width > limit[0] or len(rows) > limit[1]:
        raise ThemeError(f"{where}: at most {limit[0]} wide and {limit[1]} high")
    shades = [rgb565(color, where) for color in palette] + [0] * (PALETTE_COLORS - len(palette))
    section = struct.pack(f"<{PALETTE_COLORS}H", *shades)
    return width, len(rows), section + wallpaper_rows(rows, len(palette), where)


def asset_memory(width, height):
    return WALLPAPER_PAIR_BYTES + width // 2 * height


def wallpaper_assets(theme, base, where):
    found = []
    for kind, (key, width, height) in enumerate(WALLPAPERS):
        table = theme.get(key, base.get(key))
        if table is not None:
            wide, high, section = wallpaper_section(table, (width, height), f"{where}: {key}")
            head = struct.pack("<BBHHH", kind, 0, wide, high, 0)
            found.append((head, section, asset_memory(wide, high)))
    return found


def aligned(size):
    return (size + 3) // 4 * 4


def glyph_char(glyph, where):
    char = glyph.get("char")
    if not isinstance(char, str) or len(char) != 1 or ord(char) > 0xFFFF:
        raise ThemeError(f"{where}: each glyph names one character, char = \"A\"")
    return ord(char)


def glyph_number(glyph, key, span, where):
    value = glyph.get(key)
    if isinstance(value, bool) or not isinstance(value, int) or not span[0] <= value <= span[1]:
        raise ThemeError(f"{where}: {key} must be a whole number from {span[0]} to {span[1]}")
    return value


def glyph_levels(rows, where):
    if not isinstance(rows, list) or any(not isinstance(row, str) for row in rows):
        raise ThemeError(f"{where}: rows is a list of rows of text")
    width = len(rows[0]) if rows else 0
    if any(len(row) != width for row in rows) or width > 255 or len(rows) > 255:
        raise ThemeError(f"{where}: every row needs the same length, at most 255")
    levels = [ICON_LEVELS.find(mark) if mark != "0" else 0 for row in rows for mark in row.lower()]
    if -1 in levels:
        raise ThemeError(f"{where}: a row is made of . and 1-9, a-f, from clear to solid")
    return width, len(rows), levels


def glyph_parts(glyph, offset, where):
    if not isinstance(glyph, dict) or set(glyph) - GLYPH_KEYS:
        raise ThemeError(f"{where}: a glyph has char, left, top, advance and rows")
    code = glyph_char(glyph, where)
    where = f"{where} {glyph['char']!r}"
    width, height, levels = glyph_levels(glyph.get("rows", []), where)
    left, top = (glyph_number(glyph, key, (-128, 127), where) for key in ("left", "top"))
    advance = glyph_number(glyph, "advance", (0, 255), where)
    record = GLYPH_RECORD.pack(code, offset, width, height, left, top, advance)
    padded = levels + [0] * (len(levels) % 2)
    return code, record, bytes(first << 4 | second for first, second in zip(padded[::2], padded[1::2]))


def font_glyphs(glyphs, where):
    if not isinstance(glyphs, list) or not 1 <= len(glyphs) <= MAX_FONT_GLYPHS:
        raise ThemeError(f"{where}: glyphs is a list of 1 to {MAX_FONT_GLYPHS} glyphs")
    ordered = sorted(glyphs, key=lambda glyph: glyph_char(glyph, where))
    records, bitmap, seen = b"", b"", set()
    for glyph in ordered:
        code, record, levels = glyph_parts(glyph, len(bitmap), where)
        if code in seen:
            raise ThemeError(f"{where}: {glyph['char']!r} is given twice")
        seen.add(code)
        records, bitmap = records + record, bitmap + levels
    if len(bitmap) > 0xFFFF:
        raise ThemeError(f"{where}: the glyphs need more than 65,535 bytes")
    return len(ordered), records, bitmap


def font_asset(table, kind, limit, where):
    if not isinstance(table, dict) or set(table) - {"ascent", "descent", "glyphs"}:
        raise ThemeError(f"{where}: a font names a file, or gives ascent, descent and glyphs")
    ascent = glyph_number(table, "ascent", (1, limit), where)
    descent = glyph_number(table, "descent", (0, limit), where)
    if ascent + descent > limit:
        raise ThemeError(f"{where}: ascent and descent add up to {ascent + descent}, "
                         f"more than the {limit} this text has room for")
    count, records, bitmap = font_glyphs(table.get("glyphs"), where)
    head = struct.pack("<BBHHBB", kind, ascent, count, len(bitmap), descent, 0)
    return head, records + bitmap, aligned(len(records)) + aligned(len(bitmap))


def font_assets(theme, base, fonts, where):
    table = {**base.get("fonts", {}), **own_table(theme, "fonts", where)}
    unknown = set(table) - {name for name, _ in fonts}
    if unknown:
        raise ThemeError(f"{where}: {', '.join(sorted(unknown))} is not a font name")
    return [font_asset(table[name], FIRST_FONT_KIND + index, limit, f"{where}: font {name}")
            for index, (name, limit) in enumerate(fonts) if name in table]


def one_scene(theme, key, kinds, colors, color_table, names, where):
    table = own_table(theme, key, where)
    if not table:
        return []
    if set(table) - SCENE_KEYS or not isinstance(table.get("script"), str):
        raise ThemeError(f"{where}: {key} has a script (or a file) and may have chrome")
    try:
        code, strings = scene_compile.compile_scene(table["script"], names)
    except scene_compile.SceneError as error:
        raise ThemeError(f"{where}: {key}: {error}") from error
    chrome = own_table(table, "chrome", f"{where}: {key}")
    unknown = set(chrome) - set(colors)
    if unknown:
        raise ThemeError(f"{where}: {key}.chrome: {', '.join(sorted(unknown))} is not a colour name")
    flags = SCENE_CHROME if chrome else 0
    shades = {**color_table, **chrome}
    section = b"".join(struct.pack("<H", rgb565(shades[name], f"{where}: chrome {name}"))
                       for name in colors) if chrome else b""
    section += code + strings
    code_kind, source_kind, design_kind = kinds
    head = struct.pack("<BBHHH", code_kind, flags, len(code), len(strings), 0)
    source = table["script"].encode("utf-8")
    found = [(head, section, 0), (struct.pack("<BBHHH", source_kind, 0, 0, 0, 0), source, 0)]
    design = table.get("design")
    if design is not None:
        if not isinstance(design, str):
            raise ThemeError(f"{where}: {key}.design is the editor's text")
        found.append((struct.pack("<BBHHH", design_kind, 0, 0, 0, 0), design.encode("utf-8"), 0))
    return found


def scene_assets(theme, colors, color_table, names, where):
    found = []
    for key, *kinds in SCENES:
        found += one_scene(theme, key, kinds, colors, color_table, names, where)
    scenes = [section for head, section, _ in found if head[0] in (SCENE_KIND, SCENES[1][1])]
    total = sum((len(section) + 3) // 4 * 4 for section in scenes)
    if total > SCENE_MOST_BYTES:
        what = "the scene is" if len(scenes) == 1 else "the Now Playing and Home scenes together are"
        raise ThemeError(f"{where}: {what} {total:,} bytes; at most {SCENE_MOST_BYTES:,}")
    return found


def with_scene(theme, folder, where):
    for key, *_ in SCENES:
        table = theme.get(key)
        if not isinstance(table, dict) or "file" not in table:
            continue
        path = folder / str(table["file"])
        if not path.is_file():
            raise ThemeError(f"{where}: {key} file {table['file']!r} isn't beside the theme")
        resolved = {name: value for name, value in table.items() if name != "file"}
        resolved["script"] = path.read_text(encoding="utf-8")
        theme = {**theme, key: resolved}
    return theme


def check_memory(walls, fonts, where):
    needed = sum(memory for _, _, memory in fonts) + max((memory for _, _, memory in walls),
                                                         default=0)
    if needed > ASSET_MEMORY:
        raise ThemeError(f"{where}: the fonts and wallpaper need {needed:,} bytes of the player's "
                         f"memory, more than its {ASSET_MEMORY:,}; use a smaller or repeating "
                         "wallpaper, or fewer fonts")


def asset_table(assets, first_offset):
    entries, offset = b"", first_offset
    for head, section, _ in assets:
        entries += head + struct.pack("<II", offset, checksum(section))
        offset += len(section)
    return entries


def rasterised(path, size, limit):
    import font_build
    try:
        ranges = json.loads(FONT_RANGES.read_text(encoding="utf-8"))["character_ranges"]
        for tried in [size] if size else range(MOST_FONT_SIZE, LEAST_FONT_SIZE - 1, -1):
            font = font_build.load_font(path, tried)
            ascent, descent = font.getmetrics()
            if ascent + descent <= limit:
                break
        glyphs = [font_build.render_glyph(font, code)
                  for code in font_build.covered_codepoints(path, ranges)]
    except (OSError, ValueError, KeyError) as error:
        raise ThemeError(f"{path.name}: {error}") from None
    rows = [["".join(ICON_LEVELS[level] for level in glyph.levels[top:top + glyph.width])
             for top in range(0, glyph.width * glyph.height, max(glyph.width, 1))]
            for glyph in glyphs]
    return {"ascent": ascent, "descent": descent,
            "glyphs": [{"char": chr(glyph.codepoint), "left": glyph.left, "top": glyph.top,
                        "advance": glyph.advance, "rows": shape}
                       for glyph, shape in zip(glyphs, rows)]}


def font_file(table, limit, folder, where):
    name, size = table["file"], table.get("size")
    named = isinstance(name, str) and Path(name).name == name and set(table) <= {"file", "size"}
    sized = size is None or (not isinstance(size, bool) and isinstance(size, int)
                             and LEAST_FONT_SIZE <= size <= MOST_FONT_SIZE)
    if not named or not sized:
        raise ThemeError(f"{where}: a font file sits beside the theme; size is "
                         f"{LEAST_FONT_SIZE} to {MOST_FONT_SIZE} pixels")
    return rasterised(folder / name, size, limit)


def with_fonts(theme, fonts, folder, where):
    table = theme.get("fonts")
    if not isinstance(table, dict):
        return theme
    resolved = dict(table)
    for name, limit in fonts:
        font = table.get(name)
        if isinstance(font, dict) and "file" in font:
            resolved[name] = font_file(font, limit, folder, f"{where}: font {name}")
    return {**theme, "fonts": resolved}


def fitted(image, limit):
    from PIL import Image
    if image.width <= limit[0] and image.height <= limit[1]:
        return image.crop((0, 0, image.width - image.width % 2, image.height))
    scale = max(limit[0] / image.width, limit[1] / image.height)
    size = (max(limit[0], round(image.width * scale)), max(limit[1], round(image.height * scale)))
    left, top = (size[0] - limit[0]) // 2, (size[1] - limit[1]) // 2
    return image.resize(size, Image.LANCZOS).crop((left, top, left + limit[0], top + limit[1]))


def quantised(path, limit, toward):
    dim, background = toward
    try:
        from PIL import Image
        picture = fitted(Image.open(path).convert("RGB"), limit)
        picture = Image.blend(picture, Image.new("RGB", picture.size, background), dim / 100)
        indexed = picture.quantize(PALETTE_COLORS, method=Image.Quantize.MEDIANCUT,
                                   dither=Image.Dither.FLOYDSTEINBERG)
    except (ImportError, OSError, ValueError) as error:
        raise ThemeError(f"{path.name}: {error}") from None
    pixels, flat = indexed.tobytes(), indexed.getpalette()
    palette = ["#%02x%02x%02x" % tuple(flat[3 * index:3 * index + 3])
               for index in range(max(pixels) + 1)]
    rows = ["".join(HEX_DIGITS[value] for value in pixels[top:top + indexed.width])
            for top in range(0, len(pixels), indexed.width)]
    return {"palette": palette, "rows": rows}


def with_images(theme, base, folder, where):
    resolved = dict(theme)
    paper = {**base.get("colors", {}), **own_table(theme, "colors", where)}.get("background")
    for key, width, height in WALLPAPERS:
        table = theme.get(key)
        if not isinstance(table, dict) or "image" not in table:
            continue
        image, dim = table["image"], table.get("dim", 0)
        named = isinstance(image, str) and Path(image).name == image and set(table) <= {"image", "dim"}
        if not named or isinstance(dim, bool) or not isinstance(dim, int) or not 0 <= dim <= 90:
            raise ThemeError(f"{where}: {key} names an image beside the theme, and a dim of 0 to 90")
        if HEX_COLOR.match(paper or "") is None:
            raise ThemeError(f"{where}: {key} needs a background colour to dim toward")
        resolved[key] = quantised(folder / image, (width, height), (dim, paper))
    return resolved


def layout_number(table, key, span, where):
    value = table.get(key)
    if isinstance(value, bool) or not isinstance(value, int) or not span[0] <= value <= span[1]:
        raise ThemeError(f"{where}: {key} must be a whole number from {span[0]} to {span[1]}")
    return value


def layout_box(table, element, where):
    name, default_height = element
    keys = LAYOUT_COVER_KEYS if name == "cover" else LAYOUT_KEYS
    if not isinstance(table, dict) or set(table) - keys:
        raise ThemeError(f"{where}: a place has " + ", ".join(sorted(keys)))
    if "hide" in table and (table["hide"] is not True or len(table) != 1):
        raise ThemeError(f"{where}: hide = true stands alone")
    if "hide" in table:
        return LAYOUT_BOX.pack(0, 0, 0, default_height, 0)
    x, y = (layout_number(table, key, (-LAYOUT_REACH, LAYOUT_REACH), where) for key in ("x", "y"))
    if name == "cover":
        size = layout_number(table, "size", (1, LAYOUT_MOST_COVER), where)
        return LAYOUT_BOX.pack(x, y, size, 0, 0)
    width = layout_number(table, "width", (1, LAYOUT_REACH), where)
    height = layout_number({"height": default_height, **table}, "height", (1, 255), where)
    align = table.get("align", "left")
    if align not in ALIGNS:
        raise ThemeError(f"{where}: align is " + ", ".join(ALIGNS))
    return LAYOUT_BOX.pack(x, y, width, height, ALIGNS.index(align))


def layout_bytes(theme, base, layout, key, where):
    table = {**base.get(key, {}), **own_table(theme, key, where)}
    known = [name for name, _ in layout]
    if set(table) - set(known):
        raise ThemeError(f"{where}: {', '.join(sorted(set(table) - set(known)))} is not "
                         f"something {key} places")
    if not table:
        return 0, b""
    boxes = [layout_box(table[name], (name, height), f"{where}: {key}.{name}") if name in table
             else LAYOUT_BOX.pack(0, 0, 0, 0, LAYOUT_UNSET) for name, height in layout]
    return len(boxes), b"".join(boxes)


def accent_bytes(accent, where):
    if not isinstance(accent, dict) or set(accent) != ACCENT_KEYS:
        raise ThemeError(f"{where}: an accent has a name, selection_background, selection_top "
                         "and selection_shine")
    name = accent["name"]
    if not isinstance(name, str):
        raise ThemeError(f"{where}: an accent's name is text")
    encoded = name.encode("utf-8")
    if not encoded or len(encoded) >= ACCENT_NAME_SIZE:
        raise ThemeError(f"{where}: accent {name!r}: a name needs 1 to {ACCENT_NAME_SIZE - 1} "
                         "bytes of UTF-8")
    standard = name.lower() in ACCENT_NAMES
    shades = [rgb565(accent[key], f"{where}: accent {name}: {key}")
              for key in ("selection_background", "selection_top", "selection_shine")]
    return ACCENT.pack(*shades, ACCENT_NAMES.index(name.lower()) if standard else CUSTOM_ACCENT,
                       b"" if standard else encoded)


def accents_of(theme, where):
    accents = theme.get("accents", [])
    if not isinstance(accents, list) or len(accents) > MOST_ACCENTS:
        raise ThemeError(f"{where}: accents is a list of at most {MOST_ACCENTS} [[accents]]")
    return [accent_bytes(accent, where) for accent in accents]


def flags_of(theme, where):
    style = theme.get("cover_flow", COVER_FLOW_STYLES[0])
    if style not in COVER_FLOW_STYLES:
        raise ThemeError(f"{where}: cover_flow is " + " or ".join(map(repr, COVER_FLOW_STYLES)))
    home_art = theme.get("home_art", True)
    if not isinstance(home_art, bool):
        raise ThemeError(f"{where}: home_art is true or false")
    return (FLAG_DARK_COVER_FLOW if style == "dark" else 0) | (0 if home_art else FLAG_NO_HOME_ART)


def pack(theme, base, colors, boxes, where="theme", spacing=(), icons=(), fonts=(),
         layout=(), scene_names=None):
    check_keys(theme, where)
    color_table = merged(base, theme, "colors", colors, where)
    box_table = merged(base, theme, "boxes", boxes, where)
    selection = theme.get("selection", base.get("selection", boxes[0]))
    if selection not in boxes:
        raise ThemeError(f"{where}: selection {selection!r} is not a box name")
    spaces = spacing_bytes({**base.get("spacing", {}), **own_table(theme, "spacing", where)},
                           spacing, where)
    icon_count, icon_bytes = icon_section({**base.get("icons", {}), **own_table(theme, "icons", where)},
                                          icons, where)
    walls = wallpaper_assets(theme, base, where)
    faces = font_assets(theme, base, fonts, where)
    check_memory(walls, faces, where)
    scenes = scene_assets(theme, colors, color_table, scene_names, where) if scene_names else []
    if any(theme.get(key) for key, *_ in SCENES) and not scene_names:
        raise ThemeError(f"{where}: this packer can't read scenes")
    assets = walls + faces + scenes
    placed = [layout_bytes(theme, base, layout, key, where) for key in LAYOUTS]
    accents = accents_of(theme, where)
    flags = flags_of(theme, where)
    long_header = bool(assets) or any(count for count, _ in placed) or bool(accents) or flags
    body = struct.pack("<HHBBBB", len(colors), len(boxes), BOX_SIZE, boxes.index(selection),
                       len(spaces), icon_count)
    body += name_bytes(theme.get("name"), where)
    body += bytes([len(assets), placed[0][0], placed[1][0], len(accents), flags, 0, 0, 0]
                  if long_header else [])
    body += b"".join(struct.pack("<H", rgb565(color_table[name], f"{where}: {name}"))
                     for name in colors)
    body += b"".join(box_bytes(box_table[name], colors, f"{where}: box {name}") for name in boxes)
    body += spaces + b"".join(boxes for _, boxes in placed) + b"".join(accents)
    size = CHECKED_FROM + len(body) + ASSET_ENTRY_SIZE * len(assets) + len(icon_bytes)
    body += asset_table(assets, size) + icon_bytes
    if size > MAX_SIZE:
        raise ThemeError(f"{where}: the colours, shapes and icons take {size:,} bytes, more than the "
                         f"{MAX_SIZE:,} the player keeps for them; make some icons smaller, or draw fewer")
    header_size = ASSET_HEADER_SIZE if long_header else HEADER_SIZE
    core = MAGIC + struct.pack("<HHII", VERSION, header_size, size, checksum(body)) + body
    return core + b"".join(section for _, section, _ in assets)


CHECKS = (
    {"kind": "pair", "color": "text", "on": "background", "floor": 4.5, "what": "List text"},
    {"kind": "pair", "color": "text_secondary", "on": "background", "floor": 3.0,
     "what": "Secondary text"},
    {"kind": "pair", "color": "text_tertiary", "on": "background", "floor": 2.5,
     "what": "Faint text"},
    {"kind": "pair", "color": "chevron", "on": "background", "floor": 2.0, "what": "Row arrows"},
    {"kind": "pair", "color": "danger", "on": "background", "floor": 3.0, "what": "The low battery colour"},
    {"kind": "pair", "color": "success", "on": "background", "floor": 3.0, "what": "The charging colour"},
    {"kind": "ink", "box": "", "floor": 3.0, "what": "Text on the selected row"},
    {"kind": "ink", "box": "header", "floor": 4.5, "what": "The title on the header"},
    {"kind": "ink", "box": "toast", "floor": 4.5, "what": "Text on a toast"},
    {"kind": "ink", "box": "section", "floor": 4.5, "what": "The fast-scroll letter"},
    {"kind": "fills", "box": "progress_fill", "on": "progress_track", "floor": 2.0,
     "what": "The progress bar on its track"},
    {"kind": "wallpaper", "color": "text", "share": 10, "floor": 3.0,
     "what": "List text over the wallpaper"},
)


def channel_light(value, bits):
    wide = (value << (8 - bits) | value >> (2 * bits - 8)) / 255
    return wide / 12.92 if wide <= 0.04045 else ((wide + 0.055) / 1.055) ** 2.4


def luminance(color):
    return (0.2126 * channel_light(color >> 11, 5) + 0.7152 * channel_light(color >> 5 & 0x3F, 6)
            + 0.0722 * channel_light(color & 0x1F, 5))


def contrast(first, second):
    lighter, darker = sorted((luminance(first), luminance(second)), reverse=True)
    return (lighter + 0.05) / (darker + 0.05)


def package_wallpapers(package, table_at):
    found = []
    header = struct.unpack_from("<H", package, 6)[0]
    for index in range(package[HEADER_SIZE] if header >= ASSET_HEADER_SIZE else 0):
        kind, _, width, height, _, offset, _ = struct.unpack_from(
            "<BBHHHII", package, table_at + ASSET_ENTRY_SIZE * index)
        if kind >= FIRST_FONT_KIND:
            continue
        palette = struct.unpack_from(f"<{PALETTE_COLORS}H", package, offset)
        pixels_at = offset + 2 * PALETTE_COLORS
        counts = [0] * PALETTE_COLORS
        for pair in package[pixels_at:pixels_at + width // 2 * height]:
            counts[pair >> 4] += 1
            counts[pair & 0x0F] += 1
        found.append((palette, counts))
    return found


def package_tables(package, colors, boxes):
    header = struct.unpack_from("<H", package, 6)[0]
    color_count, box_count, box_size, selection, spaces = struct.unpack_from(
        "<HHBBB", package, CHECKED_FROM)
    values = struct.unpack_from(f"<{color_count}H", package, header)
    boxes_at = header + 2 * color_count
    rows = [package[boxes_at + index * box_size:boxes_at + (index + 1) * box_size]
            for index in range(box_count)]
    placed = package[49] + package[50] if header >= ASSET_HEADER_SIZE else 0
    accents = package[51] if header >= ASSET_HEADER_SIZE else 0
    table_at = (boxes_at + box_count * box_size + spaces + LAYOUT_BOX.size * placed +
                ACCENT.size * accents)
    wallpapers = package_wallpapers(package, table_at)
    return dict(zip(colors, values)), dict(zip(boxes, rows)), boxes[selection], values, wallpapers


def wallpaper_contrast(color, wallpapers, share):
    lowest = 21.0
    for palette, counts in wallpapers:
        ranked = sorted((contrast(color, shade), count) for shade, count in zip(palette, counts)
                        if count)
        covered = 0
        for ratio, count in ranked:
            covered += count
            if covered > sum(counts) * share / 100:
                lowest = min(lowest, ratio)
                break
    return lowest


def lowest_contrast(check, tables):
    by_name, rows, selection, values, wallpapers = tables
    if check["kind"] == "wallpaper":
        return wallpaper_contrast(by_name[check["color"]], wallpapers, check["share"])
    if check["kind"] == "pair":
        return contrast(by_name[check["color"]], by_name[check["on"]])
    box = rows[check["box"] or selection]
    if check["kind"] == "ink":
        return min(contrast(values[box[4]], values[box[0]]), contrast(values[box[4]], values[box[1]]))
    under = rows[check["on"]]
    return contrast(values[box[0]], values[under[0]])


def applies(check, colors, boxes):
    if check["kind"] == "wallpaper":
        return check["color"] in colors
    if check["kind"] == "pair":
        return check["color"] in colors and check["on"] in colors
    return all(name in boxes for name in (check["box"], check.get("on")) if name)


def lint(package, colors, boxes):
    tables = package_tables(package, colors, boxes)
    warnings = []
    for check in CHECKS:
        ratio = lowest_contrast(check, tables) if applies(check, colors, boxes) else check["floor"]
        if ratio < check["floor"]:
            warnings.append(f"{check['what']}: contrast {ratio:.1f}, below {check['floor']:.1f}")
    return warnings


def read_source(path):
    try:
        return tomllib.loads(path.read_text(encoding="utf-8"))
    except (OSError, tomllib.TOMLDecodeError) as error:
        raise ThemeError(f"{path.name}: {error}") from None


def build_all(sources, definitions, base_source=None):
    colors = names_in(definitions / "theme_colors.def")
    boxes = names_in(definitions / "theme_boxes.def")
    spacing = spacing_in(definitions / "theme_spacing.def")
    icons = icons_in(definitions / "theme_icons.def")
    fonts = fonts_in(definitions / "theme_fonts.def")
    layout = layout_in(definitions / "theme_layout.def")
    scene_folder = definitions.resolve().parents[2] / "np_faces" / "include" / "np_faces"
    scene_names = scene_compile.definitions(scene_folder) if scene_folder.is_dir() else None
    own_base = sources / (BASE_THEME + SOURCE_SUFFIX)
    base = read_source(base_source or own_base)
    packages = {}
    for path in sorted(sources.glob("*" + SOURCE_SUFFIX)):
        under = {} if path == own_base else base
        theme = with_images(read_source(path), under, path.parent, path.name)
        theme = with_fonts(theme, fonts, path.parent, path.name)
        theme = with_scene(theme, path.parent, path.name)
        packages[path.stem] = pack(theme, under, colors, boxes, path.name, spacing, icons, fonts,
                                   layout, scene_names)
    return packages


def warned(packages, definitions):
    colors = names_in(definitions / "theme_colors.def")
    boxes = names_in(definitions / "theme_boxes.def")
    unclear = 0
    for stem, package in packages.items():
        for warning in lint(package, colors, boxes):
            print(f"warning: {stem}: {warning}", file=sys.stderr)
            unclear += 1
    return unclear


def parse_arguments(argv):
    parser = argparse.ArgumentParser(description="Pack theme sources into files for the player")
    parser.add_argument("--themes", type=Path, required=True, help="folder of .toml theme sources")
    parser.add_argument("--definitions", type=Path, required=True,
                        help="folder with theme_colors.def and theme_boxes.def")
    parser.add_argument("--base", type=Path,
                        help="the theme that fills in what a source leaves out; "
                             "defaults to dark.toml in the themes folder")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true",
                        help="fail if the packed themes there differ from a fresh build")
    parser.add_argument("--strict", action="store_true",
                        help="fail if a theme is hard to read")
    return parser.parse_args(argv)


def main(argv=None):
    arguments = parse_arguments(argv)
    try:
        packages = build_all(arguments.themes, arguments.definitions, arguments.base)
    except ThemeError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    stale = []
    unclear = warned(packages, arguments.definitions)
    for stem, package in packages.items():
        path = arguments.output / (stem + PACKAGE_SUFFIX)
        print(f"{stem}: {len(package)} bytes")
        if arguments.check:
            if not path.exists() or path.read_bytes() != package:
                stale.append(path.name)
        else:
            arguments.output.mkdir(parents=True, exist_ok=True)
            path.write_bytes(package)
    if stale:
        print("error: out of date: " + ", ".join(stale), file=sys.stderr)
        return 1
    return 1 if arguments.strict and unclear else 0


if __name__ == "__main__":
    sys.exit(main())

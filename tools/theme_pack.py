import argparse
import re
import struct
import sys
import tomllib
from pathlib import Path

MAGIC = b"ETHM"
VERSION = 1
HEADER_SIZE = 48
CHECKED_FROM = 16
NAME_SIZE = 24
MAX_SIZE = 512
BOX_SIZE = 11
MAX_RADIUS = 32
HASH_START = 2166136261
HASH_PRIME = 16777619
EDGES = {"top": 1, "bottom": 2, "left": 4, "right": 8}
ALL_EDGES = 15
BYTE_FIELDS = ("border_width", "shine_height")
BOX_KEYS = {"fill", "border", "shine", "ink", "border_width", "border_edges", "shine_height", "inset",
            "radius"}
THEME_KEYS = {"name", "author", "description", "selection", "colors", "boxes", "spacing"}
HEX_COLOR = re.compile(r"^#([0-9a-fA-F]{6})$")
ENTRY = re.compile(r"^THEME_(?:COLOR|BOX)\(\w+,\s*(\w+)\)\s*$")
SPACING_ENTRY = re.compile(r"^THEME_SPACING\(\w+,\s*(\w+),\s*(\d+),\s*(\d+)\)\s*$")
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
    return bytes([top, bottom, *named, width, edges, shine_height, across, down, radius])


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


def name_bytes(name, where):
    encoded = name.encode("utf-8") if isinstance(name, str) else b""
    if not encoded or len(encoded) >= NAME_SIZE:
        raise ThemeError(f"{where}: the name needs 1 to {NAME_SIZE - 1} bytes of UTF-8")
    return encoded.ljust(NAME_SIZE, b"\x00")


def check_keys(theme, where):
    if set(theme) - THEME_KEYS:
        raise ThemeError(f"{where}: unknown entries: " + ", ".join(sorted(set(theme) - THEME_KEYS)))


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


def pack(theme, base, colors, boxes, where="theme", spacing=()):
    check_keys(theme, where)
    color_table = merged(base, theme, "colors", colors, where)
    box_table = merged(base, theme, "boxes", boxes, where)
    selection = theme.get("selection", base.get("selection", boxes[0]))
    if selection not in boxes:
        raise ThemeError(f"{where}: selection {selection!r} is not a box name")
    spaces = spacing_bytes({**base.get("spacing", {}), **theme.get("spacing", {})}, spacing, where)
    body = struct.pack("<HHBBBB", len(colors), len(boxes), BOX_SIZE, boxes.index(selection),
                       len(spaces), 0)
    body += name_bytes(theme.get("name"), where)
    body += b"".join(struct.pack("<H", rgb565(color_table[name], f"{where}: {name}"))
                     for name in colors)
    body += b"".join(box_bytes(box_table[name], colors, f"{where}: box {name}") for name in boxes)
    body += spaces
    size = CHECKED_FROM + len(body)
    if size > MAX_SIZE:
        raise ThemeError(f"{where}: {size} bytes; a theme may be at most {MAX_SIZE}")
    return MAGIC + struct.pack("<HHII", VERSION, HEADER_SIZE, size, checksum(body)) + body


def read_source(path):
    try:
        return tomllib.loads(path.read_text(encoding="utf-8"))
    except (OSError, tomllib.TOMLDecodeError) as error:
        raise ThemeError(f"{path.name}: {error}") from None


def build_all(sources, definitions):
    colors = names_in(definitions / "theme_colors.def")
    boxes = names_in(definitions / "theme_boxes.def")
    spacing = spacing_in(definitions / "theme_spacing.def")
    base = read_source(sources / (BASE_THEME + SOURCE_SUFFIX))
    packages = {}
    for path in sorted(sources.glob("*" + SOURCE_SUFFIX)):
        theme = read_source(path)
        packages[path.stem] = pack(theme, {} if path.stem == BASE_THEME else base, colors, boxes,
                                   path.name, spacing)
    return packages


def parse_arguments(argv):
    parser = argparse.ArgumentParser(description="Pack theme sources into files for the player")
    parser.add_argument("--themes", type=Path, required=True, help="folder of .toml theme sources")
    parser.add_argument("--definitions", type=Path, required=True,
                        help="folder with theme_colors.def and theme_boxes.def")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true",
                        help="fail if the packed themes there differ from a fresh build")
    return parser.parse_args(argv)


def main(argv=None):
    arguments = parse_arguments(argv)
    try:
        packages = build_all(arguments.themes, arguments.definitions)
    except ThemeError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    stale = []
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
    return 0


if __name__ == "__main__":
    sys.exit(main())

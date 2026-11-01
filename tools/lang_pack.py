"""Builds the language packs the player loads at boot (src/core/i18n.c).

The catalogue is the STRING(STR_NAME, "English") lines of src/core/include/core/strings_*.def, in the
order strings_all.def includes them. Each language is a text file assets/lang/<code>.txt of
`STR_NAME = translation` lines; blank lines and lines starting with `;` are ignored, a missing or
empty translation is filled with the English text, so every pack is complete. A pack is:
  0x00  header (12 bytes, little-endian): "ELNG", version (u16), string count (u16), size (u32)
  0x0C  one u16 offset per string, from the end of the offset table
  ...   NUL-terminated UTF-8 strings
"""
import argparse
import json
import functools
import re
import struct
import sys
from pathlib import Path

import glyph_pack

MAGIC = b"ELNG"
VERSION = 1
HEADER = struct.Struct("<4sHHI")
PACK_LIMIT = 0x3000
GLYPH_CONFIG = Path(__file__).resolve().parents[1] / "assets" / "fonts" / "glyph_pack.json"
ASCII_PRINTABLE = set(range(0x20, 0x7F))
ENGLISH = "en"
INCLUDE = re.compile(r'#include\s+"core/(strings_\w+\.def)"')
ENTRY = re.compile(r'STRING\(\s*(\w+)\s*,\s*"((?:[^"\\]|\\.)*)"\s*\)')
LINE = re.compile(r"^(\w+)\s*=\s*(.*)$")
PLACEHOLDER = re.compile(r"\{(\d)\}")
ESCAPES = {"n": b"\n", "t": b"\t", '"': b'"', "\\": b"\\", "'": b"'"}


class PackError(Exception):
    pass


def unescape(text):
    """The bytes a C string literal holds."""
    out = bytearray()
    position = 0
    while position < len(text):
        character = text[position]
        if character != "\\":
            out += character.encode("utf-8")
            position += 1
            continue
        following = text[position + 1]
        if following == "x":
            end = position + 2
            while end < len(text) and text[end] in "0123456789abcdefABCDEF":
                end += 1
            out.append(int(text[position + 2:end], 16))
            position = end
        elif following in ESCAPES:
            out += ESCAPES[following]
            position += 2
        else:
            raise PackError(f"unsupported escape \\{following}")
    return bytes(out)


def read_catalogue(directory):
    """[(id, english text)] in enum order."""
    entries = []
    index = Path(directory) / "strings_all.def"
    for line in index.read_text(encoding="utf-8").splitlines():
        match = INCLUDE.search(line)
        if not match:
            continue
        text = (Path(directory) / match.group(1)).read_text(encoding="utf-8")
        for name, literal in ENTRY.findall(text):
            entries.append((name, unescape(literal).decode("utf-8")))
    names = [name for name, _ in entries]
    duplicates = sorted({name for name in names if names.count(name) > 1})
    if duplicates:
        raise PackError("duplicate string ids: " + ", ".join(duplicates))
    return entries


def read_translations(path, catalogue):
    known = {name for name, _ in catalogue}
    translations = {}
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        if not line.strip() or line.lstrip().startswith(";"):
            continue
        match = LINE.match(line)
        if not match:
            raise PackError(f"{path.name}:{number}: expected STR_NAME = text")
        name, text = match.group(1), match.group(2).strip()
        if name not in known:
            raise PackError(f"{path.name}:{number}: unknown id {name}")
        if name in translations:
            raise PackError(f"{path.name}:{number}: {name} is translated twice")
        translations[name] = text
    return translations


def check_placeholders(code, catalogue, translations):
    for name, english in catalogue:
        text = translations.get(name, "")
        if text and sorted(PLACEHOLDER.findall(text)) != sorted(PLACEHOLDER.findall(english)):
            raise PackError(f"{code}: {name} must use the placeholders of its English text")


@functools.lru_cache(maxsize=1)
def drawable_characters():
    config = json.loads(GLYPH_CONFIG.read_text(encoding="utf-8"))
    drawable = set(ASCII_PRINTABLE)
    for face in config["faces"]:
        drawable |= glyph_pack.face_characters(config, face)
    return frozenset(drawable)


def check_glyphs(code, catalogue, translations):
    drawable = drawable_characters() | {ord(character) for _, text in catalogue for character in text}
    for name, text in translations.items():
        missing = sorted({character for character in text if ord(character) not in drawable})
        if missing:
            shown = " ".join(f"{character} U+{ord(character):04X}" for character in missing)
            raise PackError(f"{code}: {name} uses characters no glyph pack draws: {shown}")


def build_pack(catalogue, translations):
    body = bytearray()
    offsets = []
    for name, english in catalogue:
        offsets.append(len(body))
        body += (translations.get(name) or english).encode("utf-8") + b"\0"
    if len(body) > 0xFFFF:
        raise PackError("the strings are too long for 16-bit offsets")
    table = b"".join(struct.pack("<H", offset) for offset in offsets)
    size = HEADER.size + len(table) + len(body)
    if size > PACK_LIMIT:
        raise PackError(f"the pack is {size:,} bytes, over the {PACK_LIMIT:,} byte limit")
    return HEADER.pack(MAGIC, VERSION, len(catalogue), size) + table + bytes(body)


def read_languages(path):
    return json.loads(path.read_text(encoding="utf-8"))["languages"]


def build_all(catalogue_directory, language_directory):
    """{code: pack bytes} for every language, English included, and {code: (translated, total)}."""
    catalogue = read_catalogue(catalogue_directory)
    packs, coverage = {}, {}
    for language in read_languages(Path(language_directory) / "languages.json"):
        code = language["code"]
        translations = {}
        if code != ENGLISH:
            translations = read_translations(Path(language_directory) / f"{code}.txt", catalogue)
        check_placeholders(code, catalogue, translations)
        check_glyphs(code, catalogue, translations)
        packs[code] = build_pack(catalogue, translations)
        coverage[code] = (sum(1 for text in translations.values() if text), len(catalogue))
    return packs, coverage


def parse_arguments():
    parser = argparse.ArgumentParser(description="Build language packs for the player")
    parser.add_argument("--catalogue", type=Path, required=True)
    parser.add_argument("--languages", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true",
                        help="fail if the committed packs differ from a fresh build")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    try:
        packs, coverage = build_all(arguments.catalogue, arguments.languages)
    except PackError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    stale = []
    for code, pack in packs.items():
        path = arguments.output / f"{code}.bin"
        translated, total = coverage[code]
        print(f"{code}: {translated} of {total} strings, {len(pack):,} bytes")
        if arguments.check:
            if not path.exists() or path.read_bytes() != pack:
                stale.append(path.name)
        else:
            arguments.output.mkdir(parents=True, exist_ok=True)
            path.write_bytes(pack)
    if stale:
        print("error: out of date: " + ", ".join(stale), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

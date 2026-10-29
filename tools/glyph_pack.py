"""Renders CJK characters into glyph packs the player reads on demand (src/gfx/font_pack.c).

A pack face is one blob:
  0x000  header (32 bytes, little-endian):
         "EGLF", version, entry size (12), entries per index sector (42),
         glyph count, directory offset, index offset, data offset, blob size,
         CRC of everything after the header (the image CRC, tools/make_image.py)
  0x200  directory: the first codepoint (u16) of every index sector
  ...    index sectors: 42 entries of 12 bytes, sorted by codepoint, then 8 bytes of padding:
         codepoint (u16), width, height, left (s8), top (s8), advance, 0, bitmap offset (u32)
  ...    bitmaps: 4-bit coverage, row by row, high nibble first, as in src/fonts
"""
import argparse
import json
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

from fontTools.ttLib import TTFont

import font_build
import make_image

MAGIC = b"EGLF"
VERSION = 1
SECTOR_BYTES = 512
ENTRY = struct.Struct("<HBBbbBBI")
ENTRIES_PER_SECTOR = SECTOR_BYTES // ENTRY.size
HEADER = struct.Struct("<4sBBHIIIIII")
MAX_DIRECTORY = 320
MAX_BITMAP_BYTES = 144
MAX_CODEPOINT = 0xFFFF


class PackError(Exception):
    pass


@dataclass
class Face:
    name: str
    blob: bytes
    glyph_count: int
    dropped: list


def codec_characters(codec, leads, trails=(0xA1, 0xFE)):
    """Every character a double-byte codec encodes with lead bytes in [first, last]."""
    found = set()
    for lead in range(leads[0], leads[1] + 1):
        for trail in range(trails[0], trails[1] + 1):
            try:
                text = bytes([lead, trail]).decode(codec)
            except UnicodeDecodeError:
                continue
            if len(text) == 1:
                found.add(ord(text))
    return found


def in_ranges(codepoint, ranges):
    return any(first <= codepoint <= last for first, last in ranges)


def set_characters(spec):
    ranges = spec["ranges"]
    if "codec" in spec:
        return {codepoint for codepoint in codec_characters(spec["codec"], spec["leads"])
                if in_ranges(codepoint, ranges)}
    return {codepoint for first, last in ranges for codepoint in range(first, last + 1)}


def face_characters(config, face):
    """The codepoints a face spec asks for: named character sets or plain ranges."""
    wanted = set_characters({"ranges": face.get("ranges", [])})
    for name in face.get("character_sets", []):
        wanted |= set_characters(config["character_sets"][name])
    return wanted


def face_numbers(config):
    """Face names to the 1-based numbers fonts use in pack_faces (0 means none)."""
    return {face["name"]: number for number, face in enumerate(config["faces"], start=1)}


def covered(path, codepoints):
    cmap = TTFont(str(path), fontNumber=0)["cmap"].getBestCmap()
    return sorted(codepoint for codepoint in codepoints
                  if codepoint in cmap and codepoint <= MAX_CODEPOINT)


def padded(data):
    return data + bytes(-len(data) % SECTOR_BYTES)


def index_sectors(glyphs, data_offset):
    sectors = []
    offset = data_offset
    for start in range(0, len(glyphs), ENTRIES_PER_SECTOR):
        sector = bytearray()
        for glyph in glyphs[start:start + ENTRIES_PER_SECTOR]:
            sector += ENTRY.pack(glyph.codepoint, glyph.width, glyph.height, glyph.left,
                                 glyph.top, glyph.advance, 0, offset)
            offset += (glyph.width * glyph.height + 1) // 2
        sectors.append(padded(bytes(sector)))
    return sectors


def check_glyph(glyph):
    for value, low, high, name in ((glyph.width, 0, 255, "width"), (glyph.height, 0, 255, "height"),
                                   (glyph.left, -128, 127, "left"), (glyph.top, -128, 127, "top"),
                                   (glyph.advance, 0, 255, "advance")):
        if not low <= value <= high:
            raise PackError(f"U+{glyph.codepoint:04X}: {name} {value} is out of range")


def pack_glyphs(glyphs):
    """The face blob for glyphs, which must be sorted by codepoint and fit the limits."""
    if not glyphs:
        raise PackError("a face needs at least one glyph")
    sectors = (len(glyphs) + ENTRIES_PER_SECTOR - 1) // ENTRIES_PER_SECTOR
    if sectors > MAX_DIRECTORY:
        raise PackError(f"{len(glyphs)} glyphs need {sectors} index sectors, over {MAX_DIRECTORY}")
    for glyph in glyphs:
        check_glyph(glyph)
    directory = padded(b"".join(struct.pack("<H", glyph.codepoint)
                                for glyph in glyphs[::ENTRIES_PER_SECTOR]))
    directory_offset = SECTOR_BYTES
    index_offset = directory_offset + len(directory)
    data_offset = index_offset + sectors * SECTOR_BYTES
    index = b"".join(index_sectors(glyphs, data_offset))
    data = b"".join(font_build.pack_nibbles(glyph.levels) for glyph in glyphs)
    body = bytes(SECTOR_BYTES - HEADER.size) + directory + index + data
    size = HEADER.size + len(body)
    header = HEADER.pack(MAGIC, VERSION, ENTRY.size, ENTRIES_PER_SECTOR, len(glyphs),
                         directory_offset, index_offset, data_offset, size, make_image.crc32(body))
    return header + body


def build_face(spec, codepoints, font_directory):
    """Renders a face, or returns None when its font file isn't there."""
    path = font_directory / spec["file"]
    if not path.exists():
        return None
    font = font_build.load_font(path, spec["size"])
    glyphs, dropped = [], []
    for codepoint in covered(path, codepoints):
        glyph = font_build.render_glyph(font, codepoint)
        if (glyph.width * glyph.height + 1) // 2 > MAX_BITMAP_BYTES:
            dropped.append(codepoint)
        else:
            glyphs.append(glyph)
    return Face(spec["name"], pack_glyphs(glyphs), len(glyphs), dropped)


def read_face(blob):
    """Parses a face blob back into {codepoint: (width, height, left, top, advance, bitmap)}."""
    fields = HEADER.unpack_from(blob)
    magic, version, entry_size, per_sector, count, directory, index, data, size, crc = fields
    if magic != MAGIC or version != VERSION or entry_size != ENTRY.size:
        raise PackError("not a version 1 glyph pack face")
    if size != len(blob) or make_image.crc32(blob[HEADER.size:]) != crc:
        raise PackError("the face size or CRC doesn't match")
    glyphs = {}
    for position in range(count):
        sector, within = divmod(position, per_sector)
        if within == 0:
            first = struct.unpack_from("<H", blob, directory + 2 * sector)[0]
        offset = index + sector * SECTOR_BYTES + within * ENTRY.size
        codepoint, width, height, left, top, advance, _, bitmap = ENTRY.unpack_from(blob, offset)
        if within == 0 and first != codepoint:
            raise PackError(f"directory entry {sector} doesn't match its index sector")
        if bitmap < data:
            raise PackError(f"U+{codepoint:04X} points into the index")
        glyphs[codepoint] = (width, height, left, top, advance,
                             blob[bitmap:bitmap + (width * height + 1) // 2])
    return glyphs


def parse_arguments():
    parser = argparse.ArgumentParser(description="Render glyph packs for the player")
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--fonts", type=Path,
                        help="folder holding the font files the config names (default: its folder)")
    parser.add_argument("--output", type=Path, required=True, help="folder for <face>.bin")
    parser.add_argument("--check", action="store_true",
                        help="fail if a committed face differs from a fresh build")
    return parser.parse_args()


def built_faces(config, font_directory):
    """(spec, Face or None) for every face; None when its font file is missing."""
    for spec in config["faces"]:
        yield spec, build_face(spec, face_characters(config, spec), font_directory)


def main():
    arguments = parse_arguments()
    config = json.loads(arguments.config.read_text(encoding="utf-8-sig"))
    font_directory = arguments.fonts or arguments.config.parent
    stale = []
    for spec, face in built_faces(config, font_directory):
        path = arguments.output / f"{spec['name']}.bin"
        if face is None:
            print(f"{spec['name']}: skipped, {font_directory / spec['file']} is missing")
        elif arguments.check:
            if not path.exists() or path.read_bytes() != face.blob:
                print(f"out of date: {path}")
                stale.append(path)
        else:
            arguments.output.mkdir(parents=True, exist_ok=True)
            path.write_bytes(face.blob)
            dropped = ", ".join(f"U+{codepoint:04X}" for codepoint in face.dropped) or "none"
            print(f"{face.name}: {face.glyph_count} glyphs, {len(face.blob):,} bytes; "
                  f"too big: {dropped}")
    return 1 if stale else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except PackError as error:
        print(f"glyph_pack: {error}", file=sys.stderr)
        sys.exit(1)

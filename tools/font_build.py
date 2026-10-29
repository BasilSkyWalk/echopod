import argparse
import json
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

ALPHA_LEVELS = 15
FULL_ALPHA = 255
BYTES_PER_LINE = 16
MAX_BITMAP_BYTES = 0xFFFF


@dataclass
class GlyphImage:
    codepoint: int
    width: int
    height: int
    left: int
    top: int
    advance: int
    levels: list


@dataclass
class FontData:
    name: str
    ascent: int
    descent: int
    glyphs: list
    pack_faces: tuple = (0, 0)


def quantize_alpha(value):
    return (value * ALPHA_LEVELS + FULL_ALPHA // 2) // FULL_ALPHA


def pack_nibbles(levels):
    padded = list(levels) + [0] * (len(levels) % 2)
    return bytes((padded[index] << 4) | padded[index + 1] for index in range(0, len(padded), 2))


def load_font(path, size):
    return ImageFont.truetype(str(path), size)


def covered_codepoints(path, ranges):
    cmap = TTFont(str(path))["cmap"].getBestCmap()
    wanted = [codepoint for first, last in ranges for codepoint in range(first, last + 1)]
    return sorted(codepoint for codepoint in set(wanted) if codepoint in cmap)


def empty_glyph(codepoint, advance):
    return GlyphImage(codepoint, 0, 0, 0, 0, advance, [])


def render_glyph(font, codepoint):
    character = chr(codepoint)
    advance = round(font.getlength(character))
    left, upper, right, lower = font.getbbox(character, anchor="ls")
    if right <= left or lower <= upper:
        return empty_glyph(codepoint, advance)
    image = Image.new("L", (right - left, lower - upper), 0)
    ImageDraw.Draw(image).text((-left, -upper), character, font=font, fill=FULL_ALPHA, anchor="ls")
    quantized = image.point(quantize_alpha)
    ink = quantized.getbbox()
    if ink is None:
        return empty_glyph(codepoint, advance)
    cropped = quantized.crop(ink)
    return GlyphImage(codepoint, cropped.width, cropped.height, left + ink[0], -(upper + ink[1]),
                      advance, list(cropped.getdata()))


def pack_face_numbers(assets_directory):
    path = assets_directory / "glyph_pack.json"
    if not path.exists():
        return {}
    faces = json.loads(path.read_text())["faces"]
    return {face["name"]: number for number, face in enumerate(faces, start=1)}


def build_font(spec, ranges, assets_directory):
    path = assets_directory / spec["file"]
    font = load_font(path, spec["size"])
    ascent, descent = font.getmetrics()
    glyphs = [render_glyph(font, codepoint) for codepoint in covered_codepoints(path, ranges)]
    numbers = pack_face_numbers(assets_directory)
    faces = tuple(numbers[name] if name else 0 for name in spec.get("pack_faces", [None, None]))
    return FontData(spec["name"], ascent, descent, glyphs, faces)


def constant_name(name):
    return "FONT_" + name.upper()


def byte_lines(data):
    for start in range(0, len(data), BYTES_PER_LINE):
        chunk = data[start:start + BYTES_PER_LINE]
        yield "    " + ", ".join(f"0x{value:02X}" for value in chunk) + ","


def glyph_table(glyphs):
    lines = []
    bitmap = bytearray()
    for glyph in sorted(glyphs, key=lambda item: item.codepoint):
        lines.append(f"    {{{glyph.codepoint}, {len(bitmap)}, {glyph.width}, {glyph.height}, "
                     f"{glyph.left}, {glyph.top}, {glyph.advance}}},")
        bitmap += pack_nibbles(glyph.levels)
    if len(bitmap) > MAX_BITMAP_BYTES:
        raise ValueError(f"bitmap of {len(bitmap)} bytes does not fit 16-bit offsets")
    return lines, bytes(bitmap)


def font_source(font):
    prefix = font.name.upper()
    glyph_lines, bitmap = glyph_table(font.glyphs)
    bitmap_lines = list(byte_lines(bitmap)) or ["    0x00,"]
    return "\n".join([
        f"static const uint8_t {prefix}_BITMAP[] = {{",
        *bitmap_lines,
        "};",
        "",
        f"static const Glyph {prefix}_GLYPHS[] = {{",
        *glyph_lines,
        "};",
        "",
        f"const Font {constant_name(font.name)} = {{",
        f"    .glyphs = {prefix}_GLYPHS,",
        f"    .bitmap = {prefix}_BITMAP,",
        f"    .glyph_count = {len(font.glyphs)},",
        f"    .bitmap_size = {len(bitmap)},",
        f"    .ascent = {font.ascent},",
        f"    .descent = {font.descent},",
        *([f"    .pack_faces = {{{font.pack_faces[0]}, {font.pack_faces[1]}}},"]
          if any(font.pack_faces) else []),
        "};",
        "",
    ])


def header_source(names):
    declarations = [f"extern const Font {constant_name(name)};" for name in names]
    return "\n".join(["#pragma once", "", '#include "gfx/font.h"', "", *declarations, ""])


def fonts_source(fonts):
    parts = ['#include "fonts/fonts.h"', "", "#include <stdint.h>", ""]
    parts += [font_source(font) for font in fonts]
    return "\n".join(parts)


def formatted(source, style_file):
    formatter = shutil.which("clang-format")
    if not formatter:
        return source
    result = subprocess.run([formatter, f"--style=file:{style_file}", "--assume-filename=fonts.c"],
                            input=source, check=True, capture_output=True, text=True)
    return result.stdout


def generated_outputs(config_path, style_file):
    config = json.loads(config_path.read_text())
    fonts = [build_font(spec, config["character_ranges"], config_path.parent)
             for spec in config["fonts"]]
    outputs = {
        "include/fonts/fonts.h": formatted(header_source([font.name for font in fonts]), style_file),
        "fonts.c": formatted(fonts_source(fonts), style_file),
    }
    return fonts, outputs


def stale_outputs(generated, committed):
    return sorted(name for name, content in generated.items() if committed.get(name) != content)


def committed_outputs(output_directory, names):
    return {name: (output_directory / name).read_text()
            for name in names if (output_directory / name).exists()}


def write_outputs(output_directory, outputs):
    for name, content in outputs.items():
        path = output_directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)


def parse_arguments():
    parser = argparse.ArgumentParser(description="Render TTF fonts into 4-bit C glyph tables")
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--style", type=Path, required=True)
    parser.add_argument("--check", action="store_true",
                        help="fail if the committed output differs from a fresh build")
    return parser.parse_args()


def report(fonts):
    for font in fonts:
        _, bitmap = glyph_table(font.glyphs)
        print(f"{font.name}: {len(font.glyphs)} glyphs, {len(bitmap):,} bitmap bytes")


def main():
    arguments = parse_arguments()
    fonts, outputs = generated_outputs(arguments.config, arguments.style)
    if arguments.check:
        stale = stale_outputs(outputs, committed_outputs(arguments.output, outputs))
        for name in stale:
            print(f"out of date: {arguments.output / name}")
        return 1 if stale else 0
    write_outputs(arguments.output, outputs)
    report(fonts)
    return 0


if __name__ == "__main__":
    sys.exit(main())

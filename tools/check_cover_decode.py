import argparse
import io
import random
import struct
import subprocess
import sys
import zlib
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw

FULL = 112
HALF = 56
RECORD_BYTES = (FULL * FULL + HALF * HALF) * 2
EXIT_BASE = 10
RESULTS = {0: "ok", 1: "unsupported", 2: "corrupt", 3: "too big", 4: "no memory"}
PASS_STARTS = [(0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2),
               (0, 1, 1, 2)]
FUZZ_ROUNDS = 40


@dataclass
class Case:
    name: str
    data: bytes
    reference: object
    mean_limit: float = 0.0
    max_limit: int = 0
    p99_limit: int = 255
    expect: str = "ok"


def synthetic(width, height, seed, sawtooth=True):
    generator = random.Random(seed)
    image = Image.new("RGB", (width, height))
    pixels = image.load()
    for y in range(height):
        for x in range(width):
            blue = (x * 7 + y * 3) % 256 if sawtooth else (x + y) * 255 // max(1, width + height)
            pixels[x, y] = (x * 255 // max(1, width - 1), y * 255 // max(1, height - 1), blue)
    draw = ImageDraw.Draw(image)
    for _ in range(12):
        left = generator.randrange(width)
        top = generator.randrange(height)
        size = generator.randrange(4, max(5, min(width, height) // 3))
        color = tuple(generator.randrange(256) for _ in range(3))
        if generator.random() < 0.5:
            draw.ellipse((left, top, left + size, top + size), fill=color)
        else:
            draw.rectangle((left, top, left + size, top + size // 2), fill=color)
    for line in range(0, width, max(2, width // 40)):
        draw.line((line, 0, line, height // 6), fill=(255, 255, 255))
    return image


def square_crop(image):
    width, height = image.size
    side = min(width, height)
    left, top = (width - side) // 2, (height - side) // 2
    return image.crop((left, top, left + side, top + side))


def rgb565(pixel):
    red, green, blue = pixel
    return ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)


def axis_weights(span, cells):
    weights = []
    for cell in range(cells):
        low, high = cell * span / cells, (cell + 1) * span / cells
        weights.append([(index, min(index + 1, high) - max(index, low))
                        for index in range(int(low), min(span, int(high) + 1))
                        if min(index + 1, high) > max(index, low)])
    return weights


def area_average(image, cells):
    span = image.size[0]
    pixels = list(image.getdata())
    weights = axis_weights(span, cells)
    rows = []
    for y in range(span):
        row = pixels[y * span:(y + 1) * span]
        rows.append([[sum(row[x][channel] * weight for x, weight in cell) / (span / cells)
                      for channel in range(3)] for cell in weights])
    result = []
    for cell_y in weights:
        for column in range(cells):
            values = [sum(rows[y][column][channel] * weight for y, weight in cell_y) / (span / cells)
                      for channel in range(3)]
            result.append(tuple(min(255, int(value + 0.5)) for value in values))
    return result


def reference_record(image):
    cropped = square_crop(image.convert("RGB"))
    pixels = [rgb565(pixel) for pixel in area_average(cropped, FULL)]
    pixels += [rgb565(pixel) for pixel in area_average(cropped, HALF)]
    return struct.pack(f"<{len(pixels)}H", *pixels)


def expand(value):
    return ((value >> 11) * 255 // 31, (value >> 5 & 0x3F) * 255 // 63, (value & 0x1F) * 255 // 31)


def compare(record, reference):
    got = struct.unpack(f"<{len(record) // 2}H", record)
    expected = struct.unpack(f"<{len(reference) // 2}H", reference)
    differences = sorted(abs(a - b) for left, right in zip(got, expected)
                         for a, b in zip(expand(left), expand(right)))
    p99 = differences[len(differences) * 99 // 100]
    return sum(differences) / len(differences), differences[-1], p99


def jpeg_bytes(image, **options):
    buffer = io.BytesIO()
    image.save(buffer, "JPEG", **options)
    return buffer.getvalue()


def jpeg_case(name, image, **options):
    data = jpeg_bytes(image, **options)
    decoded = Image.open(io.BytesIO(data))
    decoded.load()
    return Case(name, data, decoded, mean_limit=3.0, max_limit=128, p99_limit=40)


def chunk(kind, payload):
    body = kind + payload
    return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body))


def pack_row(samples, depth):
    if depth == 16:
        return b"".join(struct.pack(">H", value * 257) for value in samples)
    if depth == 8:
        return bytes(samples)
    out = bytearray()
    per_byte = 8 // depth
    for start in range(0, len(samples), per_byte):
        byte = 0
        group = samples[start:start + per_byte]
        for index, value in enumerate(group):
            byte |= value << (8 - depth * (index + 1))
        out.append(byte)
    return bytes(out)


def paeth(left, up, up_left):
    estimate = left + up - up_left
    to_left, to_up, to_up_left = abs(estimate - left), abs(estimate - up), abs(estimate - up_left)
    if to_left <= to_up and to_left <= to_up_left:
        return left
    return up if to_up <= to_up_left else up_left


def filter_row(row, previous, kind, step):
    out = bytearray([kind])
    for index, value in enumerate(row):
        left = row[index - step] if index >= step else 0
        up = previous[index]
        up_left = previous[index - step] if index >= step else 0
        predictor = [0, left, up, (left + up) // 2, paeth(left, up, up_left)][kind]
        out.append((value - predictor) & 0xFF)
    return bytes(out)


def encode_rows(rows, depth, channels):
    step = max(1, channels * depth // 8)
    previous = bytes(len(rows[0])) if rows else b""
    encoded = bytearray()
    for index, row in enumerate(rows):
        encoded += filter_row(row, previous, index % 5, step)
        previous = row
    return bytes(encoded)


def png_bytes(samples, width, height, depth, color, channels, palette=None, interlace=False):
    passes = PASS_STARTS if interlace else [(0, 0, 1, 1)]
    raw = bytearray()
    for x0, y0, dx, dy in passes:
        columns = range(x0, width, dx)
        rows = [pack_row([value for x in columns for value in samples[y][x]], depth)
                for y in range(y0, height, dy)] if len(columns) else []
        raw += encode_rows(rows, depth, channels)
    header = struct.pack(">IIBBBBB", width, height, depth, color, 0, 0, 1 if interlace else 0)
    parts = [b"\x89PNG\r\n\x1a\n", chunk(b"IHDR", header)]
    if palette is not None:
        parts.append(chunk(b"PLTE", bytes(value for entry in palette for value in entry)))
    compressed = zlib.compress(bytes(raw), 6)
    middle = len(compressed) // 2
    parts += [chunk(b"IDAT", compressed[:middle]), chunk(b"IDAT", compressed[middle:])]
    parts.append(chunk(b"IEND", b""))
    return b"".join(parts)


def gray_case(name, width, height, depth, interlace=False, alpha=False):
    image = synthetic(width, height, width + depth).convert("L")
    top = 255 if depth >= 8 else (1 << depth) - 1
    levels = [[image.getpixel((x, y)) * top // 255 for x in range(width)] for y in range(height)]
    shown = Image.new("L", (width, height))
    shown.putdata([value * 255 // top for row in levels for value in row])
    samples = [[(value, 200) if alpha else (value,) for value in row] for row in levels]
    color = 4 if alpha else 0
    data = png_bytes(samples, width, height, depth, color, 2 if alpha else 1, interlace=interlace)
    return Case(name, data, shown, 1.0, 9)


def rgb_case(name, width, height, depth=8, interlace=False, alpha=False):
    image = synthetic(width, height, width * 3 + depth)
    pixels = list(image.getdata())
    samples = [[pixels[y * width + x] + ((x * y) % 256,) if alpha else pixels[y * width + x]
                for x in range(width)] for y in range(height)]
    channels = 4 if alpha else 3
    data = png_bytes(samples, width, height, depth, 6 if alpha else 2, channels,
                     interlace=interlace)
    return Case(name, data, image, 1.0, 9)


def palette_case(name, width, height, depth, interlace=False):
    image = synthetic(width, height, width + 11 * depth).quantize(colors=1 << depth)
    palette = image.getpalette()[:3 * (1 << depth)]
    entries = [tuple(palette[index:index + 3]) for index in range(0, len(palette), 3)]
    samples = [[(image.getpixel((x, y)),) for x in range(width)] for y in range(height)]
    data = png_bytes(samples, width, height, depth, 3, 1, palette=entries, interlace=interlace)
    return Case(name, data, image.convert("RGB"), 1.0, 9)


def pillow_png_case(name, width, height):
    image = synthetic(width, height, 77)
    buffer = io.BytesIO()
    image.save(buffer, "PNG", optimize=True)
    return Case(name, buffer.getvalue(), image, 1.0, 9)


def failing_case(name, data, expect):
    return Case(name, data, None, expect=expect)


def smooth(width, height, seed):
    return synthetic(width, height, seed, sawtooth=False)


def jpeg_cases():
    big = smooth(600, 600, 1)
    return [
        jpeg_case("jpeg_baseline_420", big, quality=90),
        jpeg_case("jpeg_baseline_444_wide", smooth(300, 200, 2), quality=95, subsampling=0),
        jpeg_case("jpeg_baseline_422_large", smooth(900, 900, 3), quality=85, subsampling=1),
        jpeg_case("jpeg_baseline_gray", smooth(500, 400, 4).convert("L"), quality=90),
        jpeg_case("jpeg_progressive_420", smooth(800, 800, 5), quality=90, progressive=True),
        jpeg_case("jpeg_progressive_444_large", smooth(1000, 900, 6), quality=90,
                  progressive=True, subsampling=0),
        jpeg_case("jpeg_progressive_gray", smooth(300, 300, 7).convert("L"), quality=90,
                  progressive=True),
        jpeg_case("jpeg_restart_markers", smooth(640, 480, 8), quality=90,
                  restart_marker_blocks=7),
        jpeg_case("jpeg_small_upscaled", smooth(90, 60, 9), quality=90),
        jpeg_case("jpeg_odd_size", smooth(333, 517, 10), quality=88),
        jpeg_case("jpeg_progressive_small", smooth(100, 150, 11), quality=90, progressive=True),
        jpeg_case("jpeg_huge", smooth(1800, 1200, 12), quality=92),
    ]


def png_cases():
    return [
        rgb_case("png_rgb8", 400, 300),
        rgb_case("png_rgba8", 256, 256, alpha=True),
        rgb_case("png_rgb16", 150, 150, depth=16),
        rgb_case("png_rgb8_interlaced", 257, 190, interlace=True),
        palette_case("png_palette8", 200, 200, 8),
        palette_case("png_palette4", 180, 130, 4),
        palette_case("png_palette2", 130, 140, 2),
        palette_case("png_palette1", 120, 120, 1),
        palette_case("png_palette4_interlaced", 99, 77, 4, interlace=True),
        gray_case("png_gray8", 300, 300, 8),
        gray_case("png_gray4", 160, 120, 4),
        gray_case("png_gray2", 113, 113, 2),
        gray_case("png_gray1", 64, 64, 1, interlace=True),
        gray_case("png_gray16", 140, 200, 16),
        gray_case("png_gray_alpha", 200, 150, 8, alpha=True),
        pillow_png_case("png_pillow", 500, 500),
    ]


def failing_cases():
    baseline = jpeg_bytes(synthetic(400, 400, 20), quality=90)
    png = png_cases()[0].data
    cmyk = jpeg_bytes(synthetic(200, 200, 21).convert("CMYK"), quality=90)
    gif = io.BytesIO()
    synthetic(50, 50, 22).save(gif, "GIF")
    return [
        failing_case("jpeg_cmyk", cmyk, "unsupported"),
        failing_case("jpeg_truncated", baseline[:len(baseline) // 2], "corrupt"),
        failing_case("png_truncated", png[:len(png) // 2], "corrupt"),
        failing_case("gif", gif.getvalue(), "unsupported"),
        failing_case("empty", b"", "unsupported"),
    ]


def run_tool(tool, image_path, record_path):
    result = subprocess.run([str(tool), str(image_path), str(record_path)], capture_output=True)
    if result.returncode == 0:
        return "ok"
    return RESULTS.get(result.returncode - EXIT_BASE, f"exit {result.returncode}")


def check_case(tool, work, case):
    image_path = work / case.name
    record_path = work / f"{case.name}.record"
    image_path.write_bytes(case.data)
    outcome = run_tool(tool, image_path, record_path)
    if outcome != case.expect:
        return f"{case.name}: expected {case.expect}, got {outcome}"
    if case.expect != "ok":
        print(f"ok   {case.name}: {outcome}")
        return None
    record = record_path.read_bytes()
    if len(record) != RECORD_BYTES:
        return f"{case.name}: record is {len(record)} bytes"
    mean, worst, p99 = compare(record, reference_record(case.reference))
    passed = mean <= case.mean_limit and worst <= case.max_limit and p99 <= case.p99_limit
    print(f"{'ok  ' if passed else 'FAIL'} {case.name}: mean {mean:.2f} (limit {case.mean_limit}), "
          f"99% {p99} (limit {case.p99_limit}), worst {worst} (limit {case.max_limit})")
    return None if passed else f"{case.name}: mean {mean:.2f}, 99% {p99}, worst {worst}"


def fuzz(tool, work, cases):
    failures = []
    for index, case in enumerate(cases):
        result = subprocess.run([str(tool), "--fuzz", str(work / case.name), str(FUZZ_ROUNDS),
                                 str(index + 1)], capture_output=True, text=True, timeout=300)
        print(result.stdout.strip())
        if result.returncode != 0 or result.stderr:
            failures.append(f"fuzz {case.name}: exit {result.returncode} {result.stderr.strip()}")
    return failures


def parse_arguments(argv):
    parser = argparse.ArgumentParser(description="Check the cover decoder against Pillow")
    parser.add_argument("--tool", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--no-fuzz", action="store_true")
    return parser.parse_args(argv)


def main(argv=None):
    arguments = parse_arguments(argv)
    arguments.work.mkdir(parents=True, exist_ok=True)
    good = jpeg_cases() + png_cases()
    failures = [failure for case in good + failing_cases()
                if (failure := check_case(arguments.tool, arguments.work, case)) is not None]
    if not arguments.no_fuzz:
        failures += fuzz(arguments.tool, arguments.work, good)
    for failure in failures:
        print(f"FAIL {failure}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

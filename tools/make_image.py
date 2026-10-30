import argparse
import datetime
import re
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

POLYNOMIAL = 0x04C10DB7
IMAGE_SIZE = 0x500004
FW_SIZE = 0x44C000
HEADER_SIZE = 0x200
OUTER_HEADER_SIZE = 0x1F8
MAGIC = b"RKnanoFW"
MAGIC_OFFSET = 0x1F8
FW_SIZE_OFFSET = 0x1F4
CODE_BASE = 0x03050000
OVERLAY_OFFSET = 0x200000
OVERLAY_BASE = 0x0307A000
OVERLAY_CAPACITY = 0x18000
OVERLAY_HEADER_SIZE = 16
OVERLAY_MAGIC = b"EOVL"
CORE1_MP3_OFFSET = 0x240000
CORE1_FLAC_OFFSET = 0x280000
CORE1_MODULE_CAPACITY = 0x20000
CORE1_MODULE_HEADER_SIZE = 512
CORE1_MODULE_MAGIC = b"EC1M"
GLYPH_TABLE_OFFSET = 0x2C0000
GLYPH_TABLE_MAGIC = b"EGPT"
GLYPH_TABLE_SIZE = 512
GLYPH_FACE_LIMIT = (GLYPH_TABLE_SIZE - 8) // 8
GLYPH_PAYLOAD_LIMIT = 0x30000
GLYPH_REGIONS = (
    (GLYPH_PAYLOAD_LIMIT, OVERLAY_OFFSET),
    (GLYPH_TABLE_OFFSET + GLYPH_TABLE_SIZE, FW_SIZE),
    (OVERLAY_OFFSET + OVERLAY_CAPACITY, CORE1_MP3_OFFSET),
    (CORE1_MP3_OFFSET + CORE1_MODULE_HEADER_SIZE + CORE1_MODULE_CAPACITY, CORE1_FLAC_OFFSET),
)
TABLE_CAPACITY = 91
TABLE_OFFSET = 0x208
ROW = struct.Struct("<4I")
PAYLOAD_OFFSET = TABLE_OFFSET + TABLE_CAPACITY * ROW.size
CODE_OFFSET = PAYLOAD_OFFSET + TABLE_CAPACITY * ROW.size
MARKER_OFFSET = FW_SIZE + HEADER_SIZE
MARKER_SIZE = 8
ROW0_TARGET = 0x03050B68
ROW2 = (0x03078CD0, 0x03079DCC, 4, 0)
CODE_LIMIT = 0x030771D4
DATA_BASE = 0x03004000
DATA_LIMIT = 0x0304C000
ENTRY_VECTOR_SIZE = 8
MAX_DATA_SIZE = 180
DATE_OFFSET = 0x00
BRAND_OFFSET = 0x10
BUILD_TAG_OFFSET = 0x30
FW1_END_OFFSET = 0x80
TEXT_FIELD_SIZE = 8
VERSION_OFFSET = 0x004
HEADER_MAGIC_OFFSET = 0x00C
HEADER_MAGIC = 0x76543210
SECTOR_FIELDS_OFFSET = 0x050
SECTOR_FIELDS = (4, 0x200, 0, 0x200, 0, 0x200, 0, 0x200)
FIELD_19C_OFFSET = 0x19C
FIELD_19C = 1
DEFAULT_VERSION = (0, 5, 0)
ELF_IDENT = b"\x7fELF\x01\x01"
ELF_HEADER = struct.Struct("<16sHHIIIIIHHHHHH")
ELF_SECTION = struct.Struct("<10I")
SHF_ALLOC = 2
SHT_NOBITS = 8
FIRMWARE_SECTIONS = (".code", ".data", ".bss")


class ImageError(Exception):
    pass


@dataclass(frozen=True)
class Firmware:
    code: bytes
    data: bytes
    bss_size: int


@dataclass(frozen=True)
class ImageParts:
    header: bytes
    rows: tuple
    payload: bytes
    marker: bytes
    overlay: bytes = b""
    core1_mp3: bytes = b""
    core1_flac: bytes = b""
    glyph_faces: tuple = ()


def crc_table():
    table = []
    for index in range(256):
        value = index << 24
        for _ in range(8):
            value = (value << 1) ^ POLYNOMIAL if value & 0x80000000 else value << 1
            value &= 0xFFFFFFFF
        table.append(value)
    return table


CRC_TABLE = crc_table()
ZERO_RUN = re.compile(rb"\x00{256,}")


def crc_bytes(value, data):
    for byte in data:
        value = ((value << 8) & 0xFFFFFFFF) ^ CRC_TABLE[(value >> 24) ^ byte]
    return value


def apply_matrix(matrix, value):
    result = 0
    for column in matrix:
        if value & 1:
            result ^= column
        value >>= 1
    return result


def zero_powers(count):
    matrix = [crc_bytes(1 << bit, b"\x00") for bit in range(32)]
    powers = []
    for _ in range(count):
        powers.append(matrix)
        matrix = [apply_matrix(matrix, column) for column in matrix]
    return powers


ZERO_POWERS = zero_powers(32)


def crc_zeros(value, count):
    for matrix in ZERO_POWERS:
        if not count:
            break
        if count & 1:
            value = apply_matrix(matrix, value)
        count >>= 1
    return value


def crc32(data):
    data = memoryview(data).cast("B")
    value, position = 0, 0
    for run in ZERO_RUN.finditer(data):
        value = crc_bytes(value, data[position:run.start()])
        value = crc_zeros(value, run.end() - run.start())
        position = run.end()
    return crc_bytes(value, data[position:])


def fw_end(fw_size):
    return (fw_size + 0x104000) // 0x100000 * 0x100000


def word(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def assemble(parts):
    if len(parts.header) != OUTER_HEADER_SIZE or len(parts.marker) != MARKER_SIZE:
        raise ImageError("header or marker has the wrong size")
    if len(parts.rows) != TABLE_CAPACITY:
        raise ImageError(f"the scatter table needs {TABLE_CAPACITY} rows")
    if PAYLOAD_OFFSET + len(parts.payload) > FW_SIZE:
        raise ImageError("the payload overlaps the header copy")
    image = bytearray(IMAGE_SIZE)
    image[:OUTER_HEADER_SIZE] = parts.header
    image[MAGIC_OFFSET:MAGIC_OFFSET + len(MAGIC)] = MAGIC
    struct.pack_into("<II", image, MAGIC_OFFSET + len(MAGIC), CODE_BASE, TABLE_CAPACITY)
    for index, row in enumerate(parts.rows):
        ROW.pack_into(image, TABLE_OFFSET + index * ROW.size, *row)
    image[PAYLOAD_OFFSET:PAYLOAD_OFFSET + len(parts.payload)] = parts.payload
    image[FW_SIZE:FW_SIZE + HEADER_SIZE] = image[:HEADER_SIZE]
    image[MARKER_OFFSET:MARKER_OFFSET + MARKER_SIZE] = parts.marker
    place_overlay(image, parts)
    place_core1_module(image, parts.core1_mp3, CORE1_MP3_OFFSET)
    place_core1_module(image, parts.core1_flac, CORE1_FLAC_OFFSET)
    place_glyph_faces(image, parts)
    end = fw_end(FW_SIZE)
    struct.pack_into("<I", image, end, crc32(image[:end]))
    return bytes(image)


def place_overlay(image, parts):
    overlay = parts.overlay
    if not overlay:
        return
    if len(overlay) > OVERLAY_CAPACITY or len(overlay) < OVERLAY_HEADER_SIZE:
        raise ImageError(f"the overlay must be {OVERLAY_HEADER_SIZE} to {OVERLAY_CAPACITY:#x} bytes")
    if PAYLOAD_OFFSET + len(parts.payload) > OVERLAY_OFFSET:
        raise ImageError("the payload reaches the overlay")
    header = OVERLAY_MAGIC + struct.pack("<III", len(overlay), crc32(overlay[OVERLAY_HEADER_SIZE:]), 0)
    image[OVERLAY_OFFSET:OVERLAY_OFFSET + len(overlay)] = header + overlay[OVERLAY_HEADER_SIZE:]


def place_core1_module(image, program, offset):
    if not program:
        return
    if len(program) > CORE1_MODULE_CAPACITY:
        raise ImageError(f"the core 1 program must fit {CORE1_MODULE_CAPACITY:#x} bytes")
    header = CORE1_MODULE_MAGIC + struct.pack("<III", len(program), crc32(program), 0)
    start = offset + CORE1_MODULE_HEADER_SIZE
    image[offset:offset + len(header)] = header
    image[start:start + len(program)] = program


def glyph_face_offsets(sizes):
    """Image offsets for faces of these sizes: largest first, first fit, 512-byte aligned."""
    free = [list(region) for region in GLYPH_REGIONS]
    offsets = [0] * len(sizes)
    for index in sorted(range(len(sizes)), key=lambda item: -sizes[item]):
        if sizes[index] == 0:
            continue
        for region in free:
            if region[1] - region[0] >= sizes[index]:
                offsets[index] = region[0]
                region[0] += (sizes[index] + 511) // 512 * 512
                break
        else:
            raise ImageError(f"glyph face {index + 1} ({sizes[index]:,} bytes) doesn't fit the image")
    return offsets


def place_glyph_faces(image, parts):
    faces = parts.glyph_faces
    if not faces:
        return
    if len(faces) > GLYPH_FACE_LIMIT:
        raise ImageError(f"the glyph table holds at most {GLYPH_FACE_LIMIT} faces")
    if PAYLOAD_OFFSET + len(parts.payload) > GLYPH_PAYLOAD_LIMIT:
        raise ImageError("the payload reaches the glyph packs")
    offsets = glyph_face_offsets([len(face) for face in faces])
    table = bytearray(GLYPH_TABLE_SIZE)
    struct.pack_into("<4sI", table, 0, GLYPH_TABLE_MAGIC, len(faces))
    for index, (face, offset) in enumerate(zip(faces, offsets)):
        struct.pack_into("<II", table, 8 + 8 * index, offset, len(face))
        image[offset:offset + len(face)] = face
    image[GLYPH_TABLE_OFFSET:GLYPH_TABLE_OFFSET + GLYPH_TABLE_SIZE] = table


def glyph_faces_in(image):
    """The faces an image's glyph table lists, as bytes (empty for an empty face)."""
    magic, count = struct.unpack_from("<4sI", image, GLYPH_TABLE_OFFSET)
    if magic != GLYPH_TABLE_MAGIC:
        return []
    entries = [struct.unpack_from("<II", image, GLYPH_TABLE_OFFSET + 8 + 8 * index)
               for index in range(count)]
    return [image[offset:offset + size] for offset, size in entries]


def split_image(image):
    rows = tuple(ROW.unpack_from(image, TABLE_OFFSET + index * ROW.size)
                 for index in range(TABLE_CAPACITY))
    return ImageParts(header=image[:OUTER_HEADER_SIZE], rows=rows,
                      payload=image[PAYLOAD_OFFSET:FW_SIZE],
                      marker=image[MARKER_OFFSET:MARKER_OFFSET + MARKER_SIZE])


def check_firmware(firmware):
    code, data = firmware.code, firmware.data
    if len(code) % 4 or len(data) % 4:
        raise ImageError("code and data must be multiples of 4 bytes")
    if len(code) < ENTRY_VECTOR_SIZE or len(data) < ENTRY_VECTOR_SIZE:
        raise ImageError("code and data must each start with an 8-byte vector table")
    if len(data) > MAX_DATA_SIZE:
        raise ImageError(f".data is {len(data)} bytes; only up to {MAX_DATA_SIZE} is proven")
    stack, reset = struct.unpack_from("<II", data)
    if stack != DATA_BASE:
        raise ImageError(f"the entry stack must be {DATA_BASE:#010x}")
    if not reset & 1:
        raise ImageError("the reset handler needs the Thumb bit")
    if not CODE_BASE <= reset & ~1 < CODE_BASE + len(code):
        raise ImageError("the reset handler must be inside the code block")
    if struct.unpack_from("<II", code) != (stack, reset):
        raise ImageError("the code vector table must match the entry vector")
    if ROW0_TARGET + len(code) > CODE_LIMIT:
        raise ImageError(f"code is {len(code)} bytes and would reach {CODE_LIMIT:#010x}")
    if DATA_BASE + len(data) + firmware.bss_size > DATA_LIMIT:
        raise ImageError(f".data and .bss would pass {DATA_LIMIT:#010x}")


def text_field(text, name):
    try:
        encoded = text.encode("ascii")
    except UnicodeEncodeError:
        raise ImageError(f"the {name} must be ASCII") from None
    if len(encoded) > TEXT_FIELD_SIZE:
        raise ImageError(f"the {name} is longer than {TEXT_FIELD_SIZE} bytes")
    return encoded.ljust(TEXT_FIELD_SIZE, b"\x00")


def bcd_date(date):
    year, month, day = date
    return int(f"{month:02d}{day:02d}{year:04d}", 16)


def fw1_end(payload):
    used = len(payload.rstrip(b"\x00"))
    return PAYLOAD_OFFSET + (used + 3) // 4 * 4


def check_version(version):
    major, minor, patch = version
    if not (0 <= major <= 0xFF and 0 <= minor <= 0xFF and 0 <= patch <= 0xFFFF):
        raise ImageError("the version must be major.minor.patch with parts up to 255, 255, 65535")


def generated_header(version):
    check_version(version)
    major, minor, patch = version
    header = bytearray(OUTER_HEADER_SIZE)
    struct.pack_into("<III", header, VERSION_OFFSET, major << 24 | minor << 16, patch,
                     HEADER_MAGIC)
    struct.pack_into("<8I", header, SECTOR_FIELDS_OFFSET, *SECTOR_FIELDS)
    struct.pack_into("<I", header, FIELD_19C_OFFSET, FIELD_19C)
    struct.pack_into("<I", header, FW_SIZE_OFFSET, FW_SIZE)
    return header


def build_image(firmware, brand, build_tag, date, overlay=b"", core1_mp3=b"", core1_flac=b"",
                version=DEFAULT_VERSION, glyph_faces=()):
    check_firmware(firmware)
    code_size, data_size = len(firmware.code), len(firmware.data)
    header = generated_header(version)
    struct.pack_into("<I", header, DATE_OFFSET, bcd_date(date))
    header[BRAND_OFFSET:BRAND_OFFSET + TEXT_FIELD_SIZE] = text_field(brand, "brand")
    header[BUILD_TAG_OFFSET:BUILD_TAG_OFFSET + TEXT_FIELD_SIZE] = text_field(build_tag, "build tag")
    payload = bytes(CODE_OFFSET - PAYLOAD_OFFSET) + firmware.code + firmware.data
    struct.pack_into("<I", header, FW1_END_OFFSET, fw1_end(payload))
    rows = [
        (ROW0_TARGET, CODE_BASE, code_size, ROW0_TARGET + code_size),
        (DATA_BASE, data_size, DATA_BASE + data_size, data_size + firmware.bss_size),
        ROW2,
    ]
    rows += [(0, 0, 0, 0)] * (TABLE_CAPACITY - len(rows))
    return assemble(ImageParts(bytes(header), tuple(rows), payload, bytes(MARKER_SIZE), overlay,
                               core1_mp3,
                               core1_flac, tuple(glyph_faces)))


def validate_image(image):
    if len(image) < HEADER_SIZE:
        return ["the file is shorter than one header"]
    problems = []
    if image[MAGIC_OFFSET:MAGIC_OFFSET + len(MAGIC)] != MAGIC:
        problems.append("the RKnanoFW magic is missing")
    fw_size = word(image, FW_SIZE_OFFSET)
    end = fw_end(fw_size)
    if fw_size < HEADER_SIZE or fw_size % 512:
        problems.append(f"the fw size {fw_size:#x} is not a positive multiple of 512")
    if fw_size + HEADER_SIZE > len(image) or end + 4 > len(image):
        problems.append("the file is shorter than its fw size says")
        return problems
    if image[fw_size:fw_size + HEADER_SIZE] != image[:HEADER_SIZE]:
        problems.append("the header copy does not match the first 512 bytes")
    if crc32(image[:end]) != word(image, end):
        problems.append("the trailer crc does not match")
    return problems


@dataclass(frozen=True)
class Section:
    kind: int
    flags: int
    address: int
    content: bytes
    size: int


def elf_sections(elf):
    if elf[:len(ELF_IDENT)] != ELF_IDENT or len(elf) < ELF_HEADER.size:
        raise ImageError("not a 32-bit little-endian ELF file")
    fields = ELF_HEADER.unpack_from(elf)
    table, count, names_index = fields[6], fields[12], fields[13]
    raw = [ELF_SECTION.unpack_from(elf, table + index * ELF_SECTION.size)
           for index in range(count)]
    names = raw[names_index]
    sections = {}
    for name, kind, flags, address, offset, size, *_ in raw:
        start = names[4] + name
        label = elf[start:elf.index(b"\x00", start)].decode("ascii")
        content = b"" if kind == SHT_NOBITS else elf[offset:offset + size]
        sections[label] = Section(kind, flags, address, content, size)
    return sections


def check_sections(sections):
    if ".code" not in sections or ".data" not in sections:
        raise ImageError("the ELF needs .code and .data sections")
    stray = [name for name, section in sections.items()
             if section.flags & SHF_ALLOC and section.size and section.kind != SHT_NOBITS
             and name not in FIRMWARE_SECTIONS]
    if stray:
        raise ImageError("loaded sections the image would drop: " + ", ".join(sorted(stray)))
    if sections[".code"].address != CODE_BASE or sections[".data"].address != DATA_BASE:
        raise ImageError(f".code must be at {CODE_BASE:#010x} and .data at {DATA_BASE:#010x}")


def firmware_from_elf(elf):
    sections = elf_sections(elf)
    check_sections(sections)
    code, data = sections[".code"].content, sections[".data"].content
    bss = sections.get(".bss")
    if bss is not None and bss.size and bss.address != DATA_BASE + len(data):
        raise ImageError(".bss must directly follow .data")
    return Firmware(code=code, data=data, bss_size=bss.size if bss is not None else 0)


def overlay_from_elf(elf):
    sections = elf_sections(elf)
    overlay = sections.get(".overlay")
    if overlay is None or overlay.address != OVERLAY_BASE:
        raise ImageError(f"the overlay ELF needs an .overlay section at {OVERLAY_BASE:#010x}")
    stray = [name for name, section in sections.items()
             if section.flags & SHF_ALLOC and section.size and name != ".overlay"]
    if stray:
        raise ImageError("the overlay may only hold code and constants: " + ", ".join(sorted(stray)))
    return overlay.content


def git_output(directory, *arguments):
    result = subprocess.run(["git", "-C", str(directory), *arguments],
                            capture_output=True, text=True, check=False)
    return result.stdout.strip() if result.returncode == 0 else None


def default_build_tag(directory):
    commit = git_output(directory, "rev-parse", "--short=7", "HEAD")
    if not commit:
        return "unknown"
    dirty = git_output(directory, "status", "--porcelain", "--untracked-files=no")
    return commit[:7] + ("+" if dirty else "")


def default_date():
    today = datetime.date.today()
    return today.year, today.month, today.day


def parse_date(text):
    try:
        year, month, day = (int(part) for part in text.split("-"))
    except ValueError:
        raise argparse.ArgumentTypeError("use YYYY-MM-DD") from None
    return year, month, day


def parse_version(text):
    try:
        major, minor, patch = (int(part) for part in text.split("."))
    except ValueError:
        raise argparse.ArgumentTypeError("use major.minor.patch") from None
    return major, minor, patch


def parse_arguments(argv):
    parser = argparse.ArgumentParser(description="Pack a device build into a 5 MiB fw1 image")
    parser.add_argument("--version", type=parse_version, default=DEFAULT_VERSION,
                        help="major.minor.patch written to the header")
    parser.add_argument("--elf", type=Path, required=True,
                        help="device ELF with .code at 0x03050000 and .data at 0x03004000")
    parser.add_argument("--brand", default="EchoPod")
    parser.add_argument("--build-tag", help="defaults to the short git commit, + when dirty")
    parser.add_argument("--date", type=parse_date, help="YYYY-MM-DD; defaults to today")
    parser.add_argument("--overlay", type=Path, help="rescan overlay ELF linked at 0x0307A000")
    parser.add_argument("--core1-mp3", type=Path, help="core 1 MP3 program, a raw binary")
    parser.add_argument("--core1-flac", type=Path, help="core 1 FLAC program, a raw binary")
    parser.add_argument("--glyph-face", type=Path, action="append", default=[],
                        help="glyph pack face, in face order; a missing file is an empty face")
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args(argv)


def main(argv=None):
    arguments = parse_arguments(argv)
    build_tag = arguments.build_tag or default_build_tag(Path(__file__).resolve().parents[1])
    date = arguments.date or default_date()
    try:
        firmware = firmware_from_elf(arguments.elf.read_bytes())
        overlay = overlay_from_elf(arguments.overlay.read_bytes()) if arguments.overlay else b""
        core1_mp3 = arguments.core1_mp3.read_bytes() if arguments.core1_mp3 else b""
        core1_flac = arguments.core1_flac.read_bytes() if arguments.core1_flac else b""
        glyph_faces = [path.read_bytes() if path.exists() else b""
                       for path in arguments.glyph_face]
        image = build_image(firmware, arguments.brand, build_tag, date, overlay, core1_mp3,
                            core1_flac, arguments.version, glyph_faces)
    except ImageError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    problems = validate_image(image)
    if problems:
        print("error: " + "; ".join(problems), file=sys.stderr)
        return 1
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_bytes(image)
    print(f"wrote {arguments.output} ({build_tag}): "
          f"code {len(firmware.code):,} bytes, "
          f"overlay {len(overlay):,} bytes, core 1 MP3 {len(core1_mp3):,} bytes, "
          f"core 1 FLAC {len(core1_flac):,} bytes, "
          f"glyph faces {sum(len(face) for face in glyph_faces):,} bytes, "
          f".data {len(firmware.data)} bytes, .bss {firmware.bss_size:,} bytes, "
          f"crc {word(image, fw_end(FW_SIZE)):#010x}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

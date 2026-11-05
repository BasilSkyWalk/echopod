"""Reports what fills the device code window, from the linker maps and the ELF.

The map gives every input section the linker placed (one per function or table with
-ffunction-sections and -fdata-sections) and which object it came from. Objects that went
through link-time optimisation all appear as one `ltrans` object, so those pieces are
attributed through the ELF's line information instead (`nm -S -l`, which needs -g).
"""
import argparse
import re
import shutil
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

CODE_WINDOW = 0x2666C
LOADED_LIMIT = 437600
OVERLAY_CAPACITY = 0x15000
CODE_SECTIONS = (".code",)
RAM_SECTIONS = (".data", ".bss", ".noinit")
INPUT_LINE = re.compile(r"^ (\S+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$")
INPUT_NAME_LINE = re.compile(r"^ (\S+)$")
INPUT_TAIL_LINE = re.compile(r"^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$")
FILL_LINE = re.compile(r"^ \*fill\*\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)")
OUTPUT_LINE = re.compile(r"^(\.[\w.]+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)")
MEMBER = re.compile(r"^(.*?)\(([^()]+)\)$")
NM_LINE = re.compile(r"^([0-9a-f]+) ([0-9a-f]+) (\S) (\S+)(?:\t(.*):(\d+))?$")
MAP_START = "Linker script and memory map"
FILL = "*fill*"
LTO_OBJECT = "ltrans"
UNKNOWN = "lto/unattributed"
SOURCE_PREFIXES = ("src/", "platform/device/", "platform/", "third_party/")


@dataclass(frozen=True)
class Piece:
    section: str
    output: str
    address: int
    size: int
    origin: str


def parse_map(text):
    """The input sections the linker placed, as Pieces, in map order."""
    pieces = []
    output = None
    pending = None
    started = False
    for line in text.splitlines():
        if not started:
            started = line.startswith(MAP_START)
            continue
        match = OUTPUT_LINE.match(line)
        if match:
            output = match.group(1)
            pending = None
            continue
        if output is None:
            continue
        if not line.startswith(" ") or line.startswith(" *("):
            pending = None
            continue
        match = FILL_LINE.match(line)
        if match:
            pieces.append(Piece(FILL, output, int(match.group(1), 16), int(match.group(2), 16),
                                FILL))
            continue
        match = INPUT_LINE.match(line)
        if match:
            pieces.append(Piece(match.group(1), output, int(match.group(2), 16),
                                int(match.group(3), 16), match.group(4).strip()))
            pending = None
            continue
        match = INPUT_NAME_LINE.match(line)
        if match and match.group(1).startswith("."):
            pending = match.group(1)
            continue
        match = INPUT_TAIL_LINE.match(line)
        if match and pending is not None:
            pieces.append(Piece(pending, output, int(match.group(1), 16), int(match.group(2), 16),
                                match.group(3).strip()))
        pending = None
    return pieces


def output_sizes(text):
    """The output sections' sizes as the map states them."""
    sizes = {}
    started = False
    for line in text.splitlines():
        if not started:
            started = line.startswith(MAP_START)
            continue
        match = OUTPUT_LINE.match(line)
        if match:
            sizes[match.group(1)] = int(match.group(3), 16)
    return sizes


def source_name(path):
    """`D:/.../src/app/player.c` -> `app/player.c`; a device file -> `board/main.c`."""
    name = path.replace("\\", "/")
    for prefix in SOURCE_PREFIXES:
        marker = "/" + prefix
        if marker in name:
            return name.split(marker, 1)[1]
    return Path(name).name


def object_name(origin):
    """`libecho_app.a(player.c.obj)` -> `app/player.c`; a plain object -> its source path."""
    if origin == FILL:
        return FILL
    match = MEMBER.match(origin)
    if match:
        archive = Path(match.group(1)).stem
        for prefix in ("libecho_", "lib"):
            if archive.startswith(prefix):
                archive = archive[len(prefix):]
                break
        return f"{archive}/{Path(match.group(2)).stem}"
    name = origin.replace("\\", "/")
    if ".dir/" in name:
        return Path(name.split(".dir/", 1)[1]).with_suffix("").as_posix()
    return Path(name).stem


def parse_nm(text):
    """Symbol address -> source file, from `nm -S -l` output."""
    files = {}
    for line in text.splitlines():
        match = NM_LINE.match(line)
        if match and match.group(5):
            files[int(match.group(1), 16)] = match.group(5)
    return files


def attribute(piece, nm_files):
    if LTO_OBJECT not in piece.origin:
        return object_name(piece.origin)
    address = piece.address
    for candidate in (address, address | 1, address & ~1):
        if candidate in nm_files:
            return source_name(nm_files[candidate])
    return UNKNOWN


def symbol_name(piece):
    for prefix in (".text.", ".rodata.", ".bss.", ".data.", ".noinit."):
        if piece.section.startswith(prefix):
            return piece.section[len(prefix):]
    return piece.section


def in_sections(pieces, outputs):
    return [piece for piece in pieces if piece.output in outputs]


def format_table(rows, headers):
    widths = [max(len(str(row[index])) for row in [headers, *rows]) for index in range(len(headers))]
    lines = []
    for row in [headers, *rows]:
        cells = [str(cell).rjust(width) if index else str(cell).ljust(width)
                 for index, (cell, width) in enumerate(zip(row, widths))]
        lines.append("  ".join(cells).rstrip())
    return lines


def limit_line(label, used, limit):
    free = limit - used
    state = "" if free >= 0 else f"  OVER BY {-free:,}"
    return f"{label}: {used:,} of {limit:,} bytes, {free:,} free{state}"


def summary(device_map, overlay_map=None):
    sizes = output_sizes(device_map)
    code = sizes.get(".code", 0)
    data = sizes.get(".data", 0)
    bss = sizes.get(".bss", 0)
    lines = [limit_line("code window", code, CODE_WINDOW),
             limit_line("loaded total (code + .data + .bss)", code + data + bss, LOADED_LIMIT)]
    if overlay_map is not None:
        overlay = output_sizes(overlay_map).get(".overlay", 0)
        lines.append(limit_line("rescan overlay", overlay, OVERLAY_CAPACITY))
    return lines


def report(device_map, overlay_map=None, nm_text="", top=25, objects=40):
    pieces = parse_map(device_map)
    nm_files = parse_nm(nm_text)
    lines = summary(device_map, overlay_map)
    lines.append("")
    code_pieces = in_sections(pieces, CODE_SECTIONS)
    ram_pieces = in_sections(pieces, RAM_SECTIONS)
    text = sum(piece.size for piece in code_pieces if piece.section.startswith(".text"))
    rodata = sum(piece.size for piece in code_pieces if piece.section.startswith(".rodata"))
    fill = sum(piece.size for piece in code_pieces if piece.section == FILL)
    code = output_sizes(device_map).get(".code", 0)
    lines.append(f"code window: .text {text:,}, .rodata {rodata:,}, fill {fill:,}, "
                 f"vectors and the rest {code - text - rodata - fill:,}")
    lines.append("")
    by_object_code = defaultdict(int)
    by_object_ram = defaultdict(int)
    for piece in code_pieces:
        by_object_code[attribute(piece, nm_files)] += piece.size
    for piece in ram_pieces:
        by_object_ram[attribute(piece, nm_files)] += piece.size
    names = sorted((set(by_object_code) | set(by_object_ram)) - {FILL},
                   key=lambda name: (-by_object_code.get(name, 0), name))
    rows = [(name, f"{by_object_code.get(name, 0):,}", f"{by_object_ram.get(name, 0):,}")
            for name in names[:objects]]
    lines.append(f"largest objects (of {len(names)}):")
    lines += format_table(rows, ("object", "code", "ram"))
    lines.append("")
    by_library = defaultdict(int)
    for name, size in by_object_code.items():
        if name != FILL:
            by_library[name.split("/")[0]] += size
    rows = [(name, f"{size:,}") for name, size in
            sorted(by_library.items(), key=lambda item: (-item[1], item[0]))]
    lines.append("code by library or directory:")
    lines += format_table(rows, ("library", "code"))
    lines.append("")
    biggest = sorted((piece for piece in code_pieces if piece.section != FILL),
                     key=lambda piece: (-piece.size, piece.address))[:top]
    rows = [(symbol_name(piece), f"{piece.size:,}", attribute(piece, nm_files))
            for piece in biggest]
    lines.append(f"largest functions and tables in the code window (top {top}):")
    lines += format_table(rows, ("symbol", "bytes", "object"))
    lines.append("")
    biggest = sorted((piece for piece in ram_pieces if piece.section != FILL),
                     key=lambda piece: (-piece.size, piece.address))[:top]
    rows = [(symbol_name(piece), f"{piece.size:,}", attribute(piece, nm_files))
            for piece in biggest]
    lines.append(f"largest .bss and .noinit objects (top {top}):")
    lines += format_table(rows, ("symbol", "bytes", "object"))
    return "\n".join(lines)


def run_nm(elf, nm="arm-none-eabi-nm"):
    """`nm -S -l` output for the ELF, or "" when nm isn't available."""
    if elf is None or not elf.exists() or shutil.which(nm) is None:
        return ""
    result = subprocess.run([nm, "-S", "-l", str(elf)], capture_output=True, text=True,
                            check=False)
    return result.stdout if result.returncode == 0 else ""


def parse_arguments(argv):
    parser = argparse.ArgumentParser(
        description="Report what fills the device code window from the linker maps")
    parser.add_argument("--build", type=Path,
                        help="a build directory such as build/release (reads its device ELF "
                             "and map and the rescan overlay map)")
    parser.add_argument("--map", type=Path, help="the device map file (instead of --build)")
    parser.add_argument("--elf", type=Path, help="the device ELF, for attributing LTO code")
    parser.add_argument("--overlay-map", type=Path, help="the rescan overlay map file")
    parser.add_argument("--nm", default="arm-none-eabi-nm", help="the nm to run on the ELF")
    parser.add_argument("--top", type=int, default=25, help="how many symbols to list")
    parser.add_argument("--objects", type=int, default=40, help="how many objects to list")
    parser.add_argument("--summary", action="store_true", help="only the lines against the limits")
    arguments = parser.parse_args(argv)
    if arguments.build is not None:
        directory = arguments.build / "platform" / "device"
        arguments.map = arguments.map or directory / "echo_device.map"
        arguments.elf = arguments.elf or directory / "echo_device.elf"
        overlay = directory / "echo_rescan_overlay.map"
        if arguments.overlay_map is None and overlay.exists():
            arguments.overlay_map = overlay
    if arguments.map is None:
        parser.error("give --build or --map")
    return arguments


def main(argv=None):
    arguments = parse_arguments(argv)
    device_map = arguments.map.read_text(encoding="utf-8", errors="replace")
    overlay_map = None
    if arguments.overlay_map is not None:
        overlay_map = arguments.overlay_map.read_text(encoding="utf-8", errors="replace")
    if arguments.summary:
        print("\n".join(summary(device_map, overlay_map)))
        return 0
    nm_text = run_nm(arguments.elf, arguments.nm)
    print(report(device_map, overlay_map, nm_text, arguments.top, arguments.objects))
    return 0


if __name__ == "__main__":
    sys.exit(main())

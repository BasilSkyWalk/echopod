import argparse
import subprocess
import sys
import tempfile
from collections import defaultdict
from pathlib import Path

HEAP_SYMBOLS = {"malloc", "calloc", "realloc", "free"}
TARGET_FLAGS = [
    "-mcpu=cortex-m3",
    "-mthumb",
    "-Os",
    "-std=c11",
    "-ffreestanding",
    "-fno-common",
    "-ffunction-sections",
    "-fdata-sections",
    "-nostdinc",
    "-Wall",
    "-Wextra",
    "-Werror",
]


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Cross-compile src/ for the Cortex-M3 and check heap use and static RAM"
    )
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--budget-bytes", type=int, required=True)
    parser.add_argument("--toolchain-prefix", default="arm-none-eabi-")
    return parser.parse_args()


def run(command):
    return subprocess.run(command, check=True, capture_output=True, text=True).stdout


def compiler_include_directory(compiler):
    return run([compiler, "-print-file-name=include"]).strip()


def include_flags(compiler, source_root):
    flags = ["-isystem", compiler_include_directory(compiler)]
    for directory in sorted(source_root.glob("*/include")):
        flags += ["-I", str(directory)]
    return flags


def compile_object(compiler, flags, source, source_root, output_directory):
    name = "_".join(source.relative_to(source_root).with_suffix(".o").parts)
    output = output_directory / name
    subprocess.run([compiler, *TARGET_FLAGS, *flags, "-c", str(source), "-o", str(output)],
                   check=True)
    return output


def section_bytes(size_tool, object_file):
    data_line = run([size_tool, str(object_file)]).splitlines()[1]
    text, data, bss = (int(value) for value in data_line.split()[:3])
    return text + data, data + bss


def heap_symbols(nm_tool, object_file):
    output = run([nm_tool, "--undefined-only", str(object_file)])
    names = {line.split()[-1] for line in output.splitlines() if line.strip()}
    return names & HEAP_SYMBOLS


def module_of(source, source_root):
    return source.relative_to(source_root).parts[0]


def audit(arguments, output_directory):
    compiler = arguments.toolchain_prefix + "gcc"
    flags = include_flags(compiler, arguments.source_root)
    flash_by_module = defaultdict(int)
    ram_by_module = defaultdict(int)
    heap_users = []
    for source in sorted(arguments.source_root.rglob("*.c")):
        object_file = compile_object(compiler, flags, source, arguments.source_root,
                                     output_directory)
        flash, ram = section_bytes(arguments.toolchain_prefix + "size", object_file)
        module = module_of(source, arguments.source_root)
        flash_by_module[module] += flash
        ram_by_module[module] += ram
        for symbol in heap_symbols(arguments.toolchain_prefix + "nm", object_file):
            heap_users.append(f"{source.relative_to(arguments.source_root)} uses {symbol}")
    return flash_by_module, ram_by_module, heap_users


def report(flash_by_module, ram_by_module, heap_users, budget_bytes):
    total = sum(ram_by_module.values())
    print(f"{'module':<8}{'flash':>12}{'static RAM':>14}")
    for module in sorted(ram_by_module):
        print(f"{module:<8}{flash_by_module[module]:>10,} B{ram_by_module[module]:>12,} B")
    print(f"{'total':<8}{sum(flash_by_module.values()):>10,} B{total:>12,} B"
          f" of {budget_bytes:,} B RAM budget")
    for user in heap_users:
        print(f"heap use forbidden: {user}")
    if total > budget_bytes:
        print("static RAM is over budget")
    return total <= budget_bytes and not heap_users


def main():
    arguments = parse_arguments()
    with tempfile.TemporaryDirectory() as output_directory:
        flash_by_module, ram_by_module, heap_users = audit(arguments, Path(output_directory))
    passed = report(flash_by_module, ram_by_module, heap_users, arguments.budget_bytes)
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())

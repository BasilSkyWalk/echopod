import argparse
import subprocess
import sys
import unicodedata
from pathlib import Path

RANGES = [
    (0x0080, 0x052F),
    (0x1D00, 0x1FFF),
    (0x2000, 0x218F),
    (0x2460, 0x24FF),
    (0x3000, 0x30FF),
    (0xF900, 0xFAFF),
    (0xFB00, 0xFB06),
    (0xFF01, 0xFF9F),
]


CASE_RANGES = [
    (0x0080, 0x058F),
    (0x10A0, 0x10FF),
    (0x13A0, 0x13FF),
    (0x1C80, 0x1CBF),
    (0x1E00, 0x1FFF),
    (0x2100, 0x218F),
    (0x24B6, 0x24E9),
    (0x2C00, 0x2D2F),
    (0xA640, 0xA7FF),
    (0xAB70, 0xABBF),
    (0xFB00, 0xFB17),
    (0xFF21, 0xFF3A),
    (0x10400, 0x1044F),
]


def case_folded(character):
    return character.casefold()


def folded(character):
    decomposed = unicodedata.normalize("NFKD", character)
    stripped = "".join(part for part in decomposed if not unicodedata.combining(part))
    return stripped.casefold()


def entries(ranges=RANGES, transform=folded):
    for first, last in ranges:
        for code_point in range(first, last + 1):
            character = chr(code_point)
            if unicodedata.category(character) == "Cn":
                continue
            result = transform(character)
            if result != character:
                yield code_point, result.encode("utf-8")


def table(prefix, rows):
    blob = bytearray()
    lines = [f"const SortFold {prefix}S[{prefix}_COUNT] = {{"]
    for code_point, data in rows:
        lines.append(f"    {{0x{code_point:04X}, {len(blob)}, {len(data)}}},")
        blob += data
    if len(blob) > 0xFFFF:
        raise ValueError("fold text no longer fits 16-bit offsets")
    lines += ["};", "", f"const uint8_t {prefix}_TEXT[{len(blob)}] = {{"]
    for start in range(0, len(blob), 12):
        lines.append("    " + ", ".join(f"0x{byte:02X}" for byte in blob[start:start + 12]) + ",")
    lines += ["};", ""]
    return lines


def render(style):
    sort_rows = list(entries())
    case_rows = list(entries(CASE_RANGES, case_folded))
    lines = ['#include "sort_fold_table.h"', ""]
    lines += table("SORT_FOLD", sort_rows) + table("CASE_FOLD", case_rows)
    header = ["#pragma once", "", "#include <stdint.h>", "", "enum {",
              f"    SORT_FOLD_COUNT = {len(sort_rows)},", f"    CASE_FOLD_COUNT = {len(case_rows)}",
              "};", "", "typedef struct SortFold {", "    uint32_t code_point;",
              "    uint16_t offset;", "    uint8_t length;", "} SortFold;", "",
              "extern const SortFold SORT_FOLDS[SORT_FOLD_COUNT];",
              "extern const uint8_t SORT_FOLD_TEXT[];",
              "extern const SortFold CASE_FOLDS[CASE_FOLD_COUNT];",
              "extern const uint8_t CASE_FOLD_TEXT[];", ""]
    return {"sort_fold_table.c": formatted("\n".join(lines), style),
            "sort_fold_table.h": formatted("\n".join(header), style)}


def formatted(text, style):
    result = subprocess.run(["clang-format", f"--style=file:{style}", "--assume-filename=x.c"],
                            input=text, capture_output=True, text=True, check=True)
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description="Generate the sort-key fold table")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--style", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    files = render(arguments.style)
    stale = [name for name, text in files.items()
             if not (arguments.output / name).exists() or
             (arguments.output / name).read_text() != text]
    if arguments.check:
        for name in stale:
            print(f"out of date: {arguments.output / name}")
        return 1 if stale else 0
    for name, text in files.items():
        (arguments.output / name).write_text(text)
    print(f"wrote {len(list(entries()))} sort folds and "
          f"{len(list(entries(CASE_RANGES, case_folded)))} case folds to {arguments.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

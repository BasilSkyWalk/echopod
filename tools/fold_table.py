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


RUN_DELTA = 0
RUN_SAME = 1
RUN_DELETE = 2
MAX_RUN = 255


def run_length(singles, start, stride, matches):
    count = 1
    while (start + count < len(singles) and count < MAX_RUN and
           singles[start + count][0] == singles[start][0] + count * stride and
           matches(singles[start + count])):
        count += 1
    return count


def best_run(singles, start):
    code_point, target = singles[start]
    best = (1, 1, RUN_DELTA if target is not None else RUN_DELETE, target)
    for stride in (1, 2):
        if target is None:
            options = [(RUN_DELETE, lambda row: row[1] is None)]
        else:
            delta = target - code_point
            options = [(RUN_DELTA, lambda row, delta=delta: row[1] is not None and row[1] - row[0] == delta),
                       (RUN_SAME, lambda row, target=target: row[1] == target)]
        for kind, matches in options:
            count = run_length(singles, start, stride, matches)
            if count > best[0]:
                best = (count, stride, kind, target)
    return best


def encode_runs(singles):
    runs, start = [], 0
    while start < len(singles):
        count, stride, kind, target = best_run(singles, start)
        code_point = singles[start][0]
        value = 0 if kind == RUN_DELETE else (target - code_point) & 0xFFFF if kind == RUN_DELTA else target
        runs.append((code_point, count, stride, kind, value))
        start += count
    return runs


def check_runs(runs, singles):
    decoded = {}
    for code_point, count, stride, kind, value in runs:
        for index in range(count):
            source = code_point + index * stride
            if kind == RUN_DELETE:
                decoded[source] = None
            elif kind == RUN_SAME:
                decoded[source] = value
            else:
                decoded[source] = (source + value) & 0xFFFF
    if decoded != dict(singles):
        raise ValueError("the fold runs do not reproduce the table")


def table(prefix, rows):
    singles, multi = [], []
    for code_point, data in rows:
        text = data.decode("utf-8")
        if code_point > 0xFFFF:
            raise ValueError("folded code points must stay in the Basic Multilingual Plane")
        if len(text) == 1 and ord(text) <= 0xFFFF:
            singles.append((code_point, ord(text)))
        elif len(text) == 0:
            singles.append((code_point, None))
        else:
            multi.append((code_point, data))
    runs = encode_runs(singles)
    check_runs(runs, singles)
    blob = bytearray()
    lines = [f"const FoldRun {prefix}_RUNS[{prefix}_RUN_COUNT] = {{"]
    for code_point, count, stride, kind, value in runs:
        lines.append(f"    {{0x{code_point:04X}, 0x{value:04X}, {count}, {stride * 4 + kind}}},")
    lines += ["};", "", f"const FoldText {prefix}_TEXTS[{prefix}_TEXT_COUNT] = {{"]
    for code_point, data in multi:
        lines.append(f"    {{0x{code_point:04X}, {len(blob)}, {len(data)}}},")
        blob += data
    lines += ["};", "", f"const uint8_t {prefix}_BYTES[{max(len(blob), 1)}] = {{"]
    for start in range(0, len(blob), 12):
        lines.append("    " + ", ".join(f"0x{byte:02X}" for byte in blob[start:start + 12]) + ",")
    lines += ["};", ""]
    return lines, len(runs), len(multi)


def render(style):
    sort_lines, sort_runs, sort_texts = table("SORT_FOLD", list(entries()))
    case_lines, case_runs, case_texts = table("CASE_FOLD", list(entries(CASE_RANGES, case_folded)))
    lines = ['#include "sort_fold_table.h"', ""] + sort_lines + case_lines
    header = ["#pragma once", "", "#include <stdint.h>", "", "enum {",
              f"    SORT_FOLD_RUN_COUNT = {sort_runs},", f"    SORT_FOLD_TEXT_COUNT = {sort_texts},",
              f"    CASE_FOLD_RUN_COUNT = {case_runs},", f"    CASE_FOLD_TEXT_COUNT = {case_texts},",
              "    FOLD_RUN_DELTA = 0,", "    FOLD_RUN_SAME = 1,", "    FOLD_RUN_DELETE = 2",
              "};", "", "typedef struct FoldRun {", "    uint16_t first;", "    uint16_t value;",
              "    uint8_t count;", "    uint8_t shape;", "} FoldRun;", "",
              "typedef struct FoldText {", "    uint16_t code_point;", "    uint16_t offset;",
              "    uint8_t length;", "} FoldText;", "",
              "extern const FoldRun SORT_FOLD_RUNS[SORT_FOLD_RUN_COUNT];",
              "extern const FoldText SORT_FOLD_TEXTS[SORT_FOLD_TEXT_COUNT];",
              "extern const uint8_t SORT_FOLD_BYTES[];",
              "extern const FoldRun CASE_FOLD_RUNS[CASE_FOLD_RUN_COUNT];",
              "extern const FoldText CASE_FOLD_TEXTS[CASE_FOLD_TEXT_COUNT];",
              "extern const uint8_t CASE_FOLD_BYTES[];", ""]
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
          f"{len(list(entries(CASE_RANGES, case_folded)))} case folds as runs to {arguments.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

"""Checks that everything defined in src/np_faces lands in the lazily loaded .np_faces section.

A function without ECHO_NP_FACES_CODE, or a table without ECHO_NP_FACES_DATA (or the entry
macro), is linked into the code window instead, where nothing can see it. A `static const` inside
a function body is a table too.
"""
import re
import sys
from pathlib import Path

CODE_MACRO = "ECHO_NP_FACES_CODE"
DATA_MACROS = ("ECHO_NP_FACES_DATA", "ECHO_NP_FACES_ENTRY")
LOCAL_TABLE = re.compile(r"\bstatic\s+const\b")
TYPE_START = re.compile(r"^\s*(typedef|struct|union|enum|extern)\b")


def scrub(text):
    """The source with preprocessor lines, strings and character constants blanked out."""
    lines, continued = [], False
    for line in text.splitlines():
        lines.append("" if continued or line.lstrip().startswith("#") else line)
        continued = line.rstrip().endswith("\\") and (continued or line.lstrip().startswith("#"))
    text = "\n".join(lines)
    return re.sub(r'"(\\.|[^"\\\n])*"|\'(\\.|[^\'\\\n])*\'', '""', text)


def kind_of(head, ender):
    if TYPE_START.match(head):
        return None
    if ender == "{" and "(" in head and "=" not in head:
        return "function"
    if ender == "{" and "=" not in head:
        return None
    if "=" in head or "(" not in head:
        return "table" if re.search(r"\bconst\b", head.split("=")[0]) else None
    return None


def line_of(text, position):
    return text.count("\n", 0, position) + 1


def violations(text):
    """(line, message) for each definition missing its section macro."""
    text = scrub(text)
    found, depth, start, head_depth = [], 0, 0, 0
    for index, char in enumerate(text):
        if char == "{":
            if depth == 0:
                found += judge(text, start, index, "{")
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                start = index + 1
        elif char == ";" and depth == 0:
            found += judge(text, start, index, ";")
            start = index + 1
    for match in LOCAL_TABLE.finditer(text):
        if depth_at(text, match.start()) > 0:
            found.append((line_of(text, match.start()), "static const inside a function"))
    return sorted(found)


def judge(text, start, end, ender):
    head = text[start:end]
    kind = kind_of(head, ender)
    where = start + len(head) - len(head.lstrip())
    if kind == "function" and CODE_MACRO not in head:
        return [(line_of(text, where), "function without " + CODE_MACRO)]
    if kind == "table" and not any(macro in head for macro in DATA_MACROS):
        return [(line_of(text, where), "const object without " + DATA_MACROS[0])]
    return []


def depth_at(text, position):
    segment = text[:position]
    return segment.count("{") - segment.count("}")


def main(argv=None):
    root = Path(argv[0] if argv else sys.argv[1])
    problems = []
    for path in sorted((root / "src" / "np_faces").glob("*.c")):
        for line, message in violations(path.read_text(encoding="utf-8")):
            problems.append(f"{path.relative_to(root)}:{line}: {message}")
    if problems:
        print("\n".join(problems))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())

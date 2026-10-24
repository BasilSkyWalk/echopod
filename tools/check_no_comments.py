import argparse
import io
import re
import sys
import tokenize
from pathlib import Path

C_SUFFIXES = {".c", ".h"}
CMAKE_SUFFIXES = {".cmake"}
CMAKE_FILE_NAMES = {"CMakeLists.txt"}
PYTHON_SUFFIXES = {".py"}
SKIPPED_DIRECTORIES = {"build", "external", "third_party"}
C_LITERAL = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
CMAKE_STRING = re.compile(r'"(?:\\.|[^"\\])*"')


def c_comment_lines(text):
    lines = []
    for number, line in enumerate(text.splitlines(), start=1):
        code = C_LITERAL.sub("", line)
        if "//" in code or "/*" in code:
            lines.append(number)
    return lines


def cmake_comment_lines(text):
    return [
        number
        for number, line in enumerate(text.splitlines(), start=1)
        if "#" in CMAKE_STRING.sub("", line)
    ]


def python_comment_lines(text):
    tokens = tokenize.generate_tokens(io.StringIO(text).readline)
    return [token.start[0] for token in tokens if token.type == tokenize.COMMENT]


def comment_finder_for(path):
    if path.suffix in C_SUFFIXES:
        return c_comment_lines
    if path.suffix in CMAKE_SUFFIXES or path.name in CMAKE_FILE_NAMES:
        return cmake_comment_lines
    if path.suffix in PYTHON_SUFFIXES:
        return python_comment_lines
    return None


def is_skipped(path, root):
    parts = path.relative_to(root).parts[:-1]
    return any(part in SKIPPED_DIRECTORIES or part.startswith(".") for part in parts)


def code_files(root):
    for path in sorted(root.rglob("*")):
        if path.is_file() and not is_skipped(path, root) and comment_finder_for(path):
            yield path


def find_violations(root):
    violations = []
    for path in code_files(root):
        finder = comment_finder_for(path)
        for line in finder(path.read_text(encoding="utf-8")):
            violations.append(f"{path.relative_to(root)}:{line}")
    return violations


def main():
    parser = argparse.ArgumentParser(description="Fail when any code file contains a comment")
    parser.add_argument("root", type=Path)
    root = parser.parse_args().root.resolve()
    violations = find_violations(root)
    for violation in violations:
        print(f"comment found: {violation}")
    print(f"{len(violations)} comment(s) in {sum(1 for _ in code_files(root))} code files")
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main())

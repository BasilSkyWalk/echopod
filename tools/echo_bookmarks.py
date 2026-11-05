import argparse
import json
import struct
import sys
import unicodedata
from pathlib import Path

MAGIC = b"EMBK"
VERSION = 1
HEADER = struct.Struct("<4sHHI")
RECORD = struct.Struct("<IIIHBB")
CAPACITY = 128
FINISHED = 1
FNV_OFFSET = 2166136261
FNV_PRIME = 16777619
SHELVES = ("audiobooks", "audiobook", "podcasts", "podcast")


def fnv1a(data, start=FNV_OFFSET):
    value = start
    for byte in data:
        value = ((value ^ byte) * FNV_PRIME) & 0xFFFFFFFF
    return value


def checksum_ok(data, end):
    stored = struct.unpack_from("<I", data, end)[0]
    return fnv1a(data[:end]) == stored


def read_bookmarks(data):
    if len(data) < HEADER.size:
        return []
    magic, version, count, _ = HEADER.unpack_from(data)
    end = HEADER.size + count * RECORD.size
    if magic != MAGIC or version != VERSION or count > CAPACITY or len(data) < end + 4:
        return []
    if not checksum_ok(data, end):
        return []
    marks = []
    for index in range(count):
        key, track_key, position_ms, chapter, flags, _ = RECORD.unpack_from(
            data, HEADER.size + index * RECORD.size)
        marks.append({"key": key, "track_key": track_key, "position_ms": position_ms,
                      "chapter": chapter, "finished": bool(flags & FINISHED)})
    return marks


def path_spellings(relative):
    spellings = {relative}
    for form in ("NFC", "NFD"):
        spellings.add(unicodedata.normalize(form, relative))
    return spellings


def known_paths(root):
    paths = {}
    for shelf in sorted(root.iterdir()) if root.is_dir() else []:
        if not shelf.is_dir() or shelf.name.casefold() not in SHELVES:
            continue
        for item in [shelf, *sorted(shelf.rglob("*"))]:
            if item.name.startswith("."):
                continue
            relative = item.relative_to(root).as_posix()
            for spelling in path_spellings(relative):
                paths.setdefault(fnv1a(spelling.encode("utf-8")), relative)
    return paths


def describe(marks, paths):
    rows = []
    for mark in marks:
        rows.append({
            "path": paths.get(mark["key"]),
            "track": paths.get(mark["track_key"]),
            "position_ms": mark["position_ms"],
            "chapter": mark["chapter"],
            "finished": mark["finished"],
        })
    return rows


def clock(milliseconds):
    seconds = milliseconds // 1000
    return f"{seconds // 3600}:{seconds // 60 % 60:02d}:{seconds % 60:02d}"


def main(arguments=None):
    parser = argparse.ArgumentParser(
        description="List the audiobook and podcast bookmarks EchoPod keeps on a card.")
    parser.add_argument("card", help="the card's root folder, for example E:\\ or /media/card")
    parser.add_argument("--finished", action="store_true",
                        help="print only the paths of finished episodes and books, one per line")
    parser.add_argument("--json", action="store_true", help="print every bookmark as JSON")
    options = parser.parse_args(arguments)
    root = Path(options.card)
    source = root / ".echo" / "bookmarks.bin"
    if not source.is_file():
        print(f"no bookmarks file at {source}", file=sys.stderr)
        return 1
    rows = describe(read_bookmarks(source.read_bytes()), known_paths(root))
    if options.json:
        print(json.dumps(rows, indent=2, ensure_ascii=False))
    elif options.finished:
        for row in rows:
            if row["finished"] and (row["path"] or row["track"]):
                print(row["path"] or row["track"])
    else:
        for row in rows:
            state = "finished" if row["finished"] else clock(row["position_ms"])
            print(f"{state:>9}  {row['track'] or row['path'] or '(not on this card)'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

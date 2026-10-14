import argparse
import sys
from pathlib import Path

import cover_export
import library_index
from library_fixture import FIXTURE_TRACKS
from PIL import Image

FIXTURE_COLORS = {0: (255, 0, 0), 2: (0, 0, 255)}


def fixture_bytes():
    sources = cover_export.album_sources(library_index.build_library(FIXTURE_TRACKS))
    images = {sources[album].key: Image.new("RGB", (8, 8), color)
              for album, color in FIXTURE_COLORS.items()}
    return cover_export.encode_covers(images)


def parse_arguments():
    parser = argparse.ArgumentParser(description="Write the covers file used by the C tests")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true",
                        help="fail if the committed fixture differs from a fresh build")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    data = fixture_bytes()
    if arguments.check:
        current = arguments.output.read_bytes() if arguments.output.exists() else b""
        if current != data:
            print(f"out of date: {arguments.output}")
            return 1
        return 0
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_bytes(data)
    print(f"wrote {len(data):,} bytes to {arguments.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

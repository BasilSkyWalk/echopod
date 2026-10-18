import argparse
from pathlib import Path

SAVE_FILES = ("settings.bin", "resume.bin", "favorites.bin")
SAVE_FILE_SIZE = 4096
LIBRARY_FILE = "library.db"
LIBRARY_FILE_SIZE = 4 * 1024 * 1024


def prepare_file(path, size):
    data = path.read_bytes() if path.is_file() else b""
    if len(data) < size:
        path.write_bytes(data + bytes(size - len(data)))


def prepare(echo_directory):
    echo_directory.mkdir(parents=True, exist_ok=True)
    for name in SAVE_FILES:
        prepare_file(echo_directory / name, SAVE_FILE_SIZE)
    prepare_file(echo_directory / LIBRARY_FILE, LIBRARY_FILE_SIZE)


def main():
    parser = argparse.ArgumentParser(
        description="Create the fixed-size files the player overwrites to save its state"
    )
    parser.add_argument("--echo", type=Path, required=True, help="the card's /.echo folder")
    arguments = parser.parse_args()
    prepare(arguments.echo)


if __name__ == "__main__":
    main()

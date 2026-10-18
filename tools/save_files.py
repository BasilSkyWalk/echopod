import argparse
from pathlib import Path

SAVE_FILES = ("settings.bin", "resume.bin", "favorites.bin")
SAVE_FILE_SIZE = 4096


def prepare_file(path):
    data = path.read_bytes() if path.is_file() else b""
    if len(data) < SAVE_FILE_SIZE:
        path.write_bytes(data + bytes(SAVE_FILE_SIZE - len(data)))


def prepare(echo_directory):
    echo_directory.mkdir(parents=True, exist_ok=True)
    for name in SAVE_FILES:
        prepare_file(echo_directory / name)


def main():
    parser = argparse.ArgumentParser(
        description="Create the fixed-size files the player overwrites to save its state"
    )
    parser.add_argument("--echo", type=Path, required=True, help="the card's /.echo folder")
    arguments = parser.parse_args()
    prepare(arguments.echo)


if __name__ == "__main__":
    main()

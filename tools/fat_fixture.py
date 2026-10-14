import argparse
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

SECTOR = 512
SPARSE_MAGIC = b"EMSP"
SPARSE_HEADER = struct.Struct("<4sII")
VOLUME_NAME = "ECHOCARD"
MANY_FILES = 120

IMAGES = {
    "fat32_mbr": {"megabytes": 64, "layout": "mbr", "format": "MS-DOS FAT32"},
    "fat16_superfloppy": {"megabytes": 16, "layout": "superfloppy", "format": "16"},
}


def pattern(seed, length):
    return bytes((index * 31 + seed) & 0xFF for index in range(length))


FIXTURE_FILES = [
    (".echo/library.db", 1, 3000),
    ("README.TXT", 2, 100),
    ("notes.txt", 3, 700),
    ("Music/Sigur Rós/Ágætis byrjun/01 Intro.flac", 4, 9000),
    ("Music/Sigur Rós/Ágætis byrjun/02 Svefn-g-englar with a long name.flac", 5, 20000),
    ("Music/🎵 Emoji/track.mp3", 6, 1500),
    ("Music/Empty.wav", 7, 0),
]


def run(*command):
    return subprocess.run(command, check=True, capture_output=True, text=True).stdout


def attach(image):
    output = run("hdiutil", "attach", "-imagekey", "diskimage-class=CRawDiskImage", "-nomount",
                 str(image))
    return output.split()[0]


def format_disk(device, settings):
    if settings["layout"] == "mbr":
        run("diskutil", "partitionDisk", device, "1", "MBR", settings["format"], VOLUME_NAME,
            "100%")
    else:
        run("newfs_msdos", "-F", settings["format"], "-v", VOLUME_NAME, device)
        run("diskutil", "mount", device)


def mount_point():
    return Path("/Volumes") / VOLUME_NAME


def fill_volume(root):
    (root / ".metadata_never_index").write_bytes(b"")
    for relative, seed, length in FIXTURE_FILES:
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(pattern(seed, length))
    many = root / "Many"
    many.mkdir()
    for index in range(MANY_FILES):
        (many / f"file number {index:03d}.txt").write_bytes(pattern(100 + index, 64))


def sparse_bytes(image_bytes):
    sectors = len(image_bytes) // SECTOR
    entries = []
    for lba in range(sectors):
        block = image_bytes[lba * SECTOR:(lba + 1) * SECTOR]
        if any(block):
            entries.append(struct.pack("<I", lba) + block)
    return SPARSE_HEADER.pack(SPARSE_MAGIC, sectors, len(entries)) + b"".join(entries)


def build_image(name, settings, output_directory):
    with tempfile.TemporaryDirectory() as directory:
        image = Path(directory) / f"{name}.img"
        with image.open("wb") as handle:
            handle.truncate(settings["megabytes"] * 1024 * 1024)
        device = attach(image)
        try:
            format_disk(device, settings)
            fill_volume(mount_point())
        finally:
            run("diskutil", "eject", device)
        data = sparse_bytes(image.read_bytes())
    output = output_directory / f"{name}.sparse"
    output.write_bytes(data)
    print(f"wrote {output}: {len(data):,} bytes")


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Make FAT card images with macOS's own tools, stored as sparse sector lists")
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    if mount_point().exists():
        print(f"error: {mount_point()} is already mounted", file=sys.stderr)
        return 1
    arguments.output.mkdir(parents=True, exist_ok=True)
    for name, settings in IMAGES.items():
        build_image(name, settings, arguments.output)
    return 0


if __name__ == "__main__":
    sys.exit(main())

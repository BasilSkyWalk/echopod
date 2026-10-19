import argparse
import subprocess
import sys
from pathlib import Path

EXFAT_NAME = b"EXFAT   "
EXFAT_NAME_OFFSET = 3
EXFAT_OK = "appears to be OK"


def is_exfat(image):
    with image.open("rb") as handle:
        boot = handle.read(16)
    return boot[EXFAT_NAME_OFFSET:EXFAT_NAME_OFFSET + len(EXFAT_NAME)] == EXFAT_NAME


def run(*command):
    return subprocess.run(command, capture_output=True, text=True)


def fat_problems(output):
    return [line for line in output.splitlines() if line.rstrip().endswith("no")]


def check_fat(image):
    result = run("fsck_msdos", "-n", str(image))
    problems = fat_problems(result.stdout + result.stderr)
    if result.returncode != 0 or problems:
        return result.stdout + result.stderr
    return None


def check_exfat(image):
    attached = run("diskutil", "image", "attach", "--noMount", str(image))
    if attached.returncode != 0:
        return attached.stdout + attached.stderr
    device = attached.stdout.split()[0]
    try:
        result = run("fsck_exfat", "-n", device.replace("/dev/disk", "/dev/rdisk"))
    finally:
        run("diskutil", "eject", device)
    if result.returncode != 0 or EXFAT_OK not in result.stdout:
        return result.stdout + result.stderr
    return None


def check_image(image):
    return check_exfat(image) if is_exfat(image) else check_fat(image)


def parse_arguments(argv):
    parser = argparse.ArgumentParser(
        description="Check card images with macOS's own fsck_msdos and fsck_exfat")
    parser.add_argument("directory", type=Path, help="folder of .img volume images")
    parser.add_argument("--expect", type=int, required=True, help="how many images must exist")
    return parser.parse_args(argv)


def main(argv=None):
    arguments = parse_arguments(argv)
    images = sorted(arguments.directory.glob("*.img"))
    if len(images) != arguments.expect:
        print(f"expected {arguments.expect} images, found {len(images)}", file=sys.stderr)
        return 1
    failures = 0
    for image in images:
        report = check_image(image)
        if report is None:
            print(f"ok {image.name}")
        else:
            failures += 1
            print(f"FAIL {image.name}\n{report}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

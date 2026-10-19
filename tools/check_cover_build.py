import argparse
import io
import shutil
import struct
import subprocess
import sys
from pathlib import Path

from mutagen import id3
from mutagen.flac import FLAC, Picture
from mutagen.mp4 import MP4, MP4Cover

import check_cover_decode
import cover_export

HEADER = struct.Struct("<4sHHHHI")
ENTRY = struct.Struct("<II")
RECORD_BYTES = check_cover_decode.RECORD_BYTES


def image(width, height, seed):
    return check_cover_decode.smooth(width, height, seed)


def encoded(picture, kind):
    buffer = io.BytesIO()
    picture.save(buffer, kind, quality=90) if kind == "JPEG" else picture.save(buffer, kind)
    return buffer.getvalue()


def flac_track(path, source, album, number, picture=None):
    shutil.copyfile(source / "tagged.flac", path)
    audio = FLAC(path)
    audio.delete()
    audio.clear_pictures()
    audio["album"], audio["albumartist"], audio["artist"] = album, "Tester", "Tester"
    audio["title"], audio["tracknumber"] = f"{album} {number}", str(number)
    if picture is not None:
        block = Picture()
        block.type, block.mime, block.data = 3, "image/jpeg", picture
        audio.add_picture(block)
    audio.save()


def mp3_track(path, source, album, pictures):
    shutil.copyfile(source / "id3v24.mp3", path)
    tags = id3.ID3()
    for frame in (id3.TALB(text=album), id3.TPE2(text="Tester"), id3.TPE1(text="Tester"),
                  id3.TIT2(text=f"{album} 1"), id3.TRCK(text="1")):
        tags.add(frame)
    for index, (kind, mime, data) in enumerate(pictures):
        tags.add(id3.APIC(encoding=0, mime=mime, type=kind, desc=str(index), data=data))
    tags.save(path)


def mp4_track(path, source, album, picture):
    shutil.copyfile(source / "aac.m4a", path)
    audio = MP4(path)
    audio.delete()
    audio["\xa9alb"], audio["aART"], audio["\xa9ART"] = [album], ["Tester"], ["Tester"]
    audio["\xa9nam"], audio["trkn"] = [f"{album} 1"], [(1, 0)]
    audio["covr"] = [MP4Cover(picture, imageformat=MP4Cover.FORMAT_JPEG)]
    audio.save()


def build_music(root, source):
    expected = {}
    front, other, folder_jpeg = image(600, 600, 31), image(300, 200, 32), image(400, 400, 33)
    late_front, disc_two, mp4 = image(500, 500, 34), image(260, 260, 35), image(320, 320, 36)
    (root / "Front").mkdir(parents=True)
    flac_track(root / "Front" / "01.flac", source, "Front", 1, encoded(front, "JPEG"))
    flac_track(root / "Front" / "02.flac", source, "Front", 2, encoded(other, "JPEG"))
    expected["Front"] = encoded(front, "JPEG")
    (root / "Other").mkdir()
    mp3_track(root / "Other" / "01.mp3", source, "Other", [(0, "image/png", encoded(other, "PNG"))])
    expected["Other"] = encoded(other, "PNG")
    (root / "Late").mkdir()
    mp3_track(root / "Late" / "01.mp3", source, "Late",
              [(0, "image/png", encoded(other, "PNG")), (3, "image/jpeg", encoded(late_front, "JPEG"))])
    expected["Late"] = encoded(late_front, "JPEG")
    (root / "Folder").mkdir()
    flac_track(root / "Folder" / "01.flac", source, "Folder", 1)
    (root / "Folder" / "folder.png").write_bytes(encoded(other, "PNG"))
    (root / "Folder" / "Cover.JPG").write_bytes(encoded(folder_jpeg, "JPEG"))
    expected["Folder"] = encoded(folder_jpeg, "JPEG")
    (root / "Split" / "CD1").mkdir(parents=True)
    (root / "Split" / "CD2").mkdir()
    flac_track(root / "Split" / "CD1" / "01.flac", source, "Split", 1)
    flac_track(root / "Split" / "CD2" / "02.flac", source, "Split", 2)
    (root / "Split" / "CD2" / "folder.jpg").write_bytes(encoded(disc_two, "JPEG"))
    expected["Split"] = encoded(disc_two, "JPEG")
    (root / "Mp4").mkdir()
    mp4_track(root / "Mp4" / "01.m4a", source, "Mp4", encoded(mp4, "JPEG"))
    expected["Mp4"] = encoded(mp4, "JPEG")
    (root / "None").mkdir()
    flac_track(root / "None" / "01.flac", source, "None", 1)
    return expected


def read_covers(data):
    magic, version, count, full, half, _ = HEADER.unpack_from(data, 0)
    if magic != b"EMCV" or version != 1 or (full, half) != (112, 56):
        raise ValueError("bad covers header")
    entries = [ENTRY.unpack_from(data, HEADER.size + index * ENTRY.size) for index in range(count)]
    if [key for key, _ in entries] != sorted(key for key, _ in entries):
        raise ValueError("index not sorted")
    return {key: data[offset:offset + RECORD_BYTES] for key, offset in entries}


def check(covers, expected):
    failures = []
    if set(covers) != {cover_export.album_key("Tester", album) for album in expected}:
        failures.append(f"covers for {len(covers)} albums, expected {len(expected)}")
    for album, data in expected.items():
        record = covers.get(cover_export.album_key("Tester", album))
        if record is None:
            failures.append(f"{album}: no cover")
            continue
        reference = check_cover_decode.reference_record(cover_export.load_image(data))
        mean, worst, p99 = check_cover_decode.compare(record, reference)
        print(f"{album}: mean {mean:.2f}, 99% {p99}, worst {worst}")
        if mean > 4.0 or p99 > 48:
            failures.append(f"{album}: mean {mean:.2f}, 99% {p99}")
    return failures


def parse_arguments(argv):
    parser = argparse.ArgumentParser(description="Check covers built on the device side")
    parser.add_argument("--tool", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    return parser.parse_args(argv)


def main(argv=None):
    arguments = parse_arguments(argv)
    shutil.rmtree(arguments.work, ignore_errors=True)
    music = arguments.work / "music"
    output = arguments.work / "output"
    output.mkdir(parents=True)
    expected = build_music(music, arguments.fixtures / "tags")
    result = subprocess.run([str(arguments.tool), str(music), str(output)], capture_output=True,
                            text=True)
    print(result.stdout.strip())
    if result.returncode != 0:
        print(f"FAIL tool exit {result.returncode}: {result.stderr}", file=sys.stderr)
        return 1
    failures = check(read_covers((output / "covers.bin").read_bytes()), expected)
    for failure in failures:
        print(f"FAIL {failure}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

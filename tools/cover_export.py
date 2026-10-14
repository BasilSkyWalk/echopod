import argparse
import io
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

import library_index

MAGIC = b"EMCV"
VERSION = 1
FULL_SIZE = 112
HALF_SIZE = 56
HEADER = struct.Struct("<4sHHHHI")
ENTRY = struct.Struct("<II")
FNV_OFFSET = 2166136261
FNV_PRIME = 16777619
KEY_SEPARATOR = b"\x1f"
KEY_NAME_LIMIT = 255
FRONT_COVER = 3
FOLDER_IMAGE_STEMS = ("cover", "folder", "front", "albumart")
FOLDER_IMAGE_SUFFIXES = (".jpg", ".jpeg", ".png")
MAX_COVERS = 0xFFFF


@dataclass
class AlbumSource:
    key: int
    title: str
    artist: str
    paths: list = field(default_factory=list)


def fnv1a(data, start=FNV_OFFSET):
    value = start
    for byte in data:
        value = ((value ^ byte) * FNV_PRIME) & 0xFFFFFFFF
    return value


def album_key(album_artist, title):
    value = fnv1a(album_artist.encode("utf-8")[:KEY_NAME_LIMIT])
    value = fnv1a(KEY_SEPARATOR, value)
    return fnv1a(title.encode("utf-8")[:KEY_NAME_LIMIT], value)


def rgb565(red, green, blue):
    return ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)


def square_crop(image):
    width, height = image.size
    side = min(width, height)
    left, top = (width - side) // 2, (height - side) // 2
    return image.crop((left, top, left + side, top + side))


def cover_pixels(image, size):
    from PIL import Image
    resized = square_crop(image.convert("RGB")).resize((size, size), Image.Resampling.LANCZOS)
    return [rgb565(*pixel) for pixel in resized.getdata()]


def embedded_pictures(path):
    from mutagen import File
    audio = File(path)
    if audio is None:
        return []
    pictures = [(picture.type, picture.data) for picture in getattr(audio, "pictures", [])]
    tags = getattr(audio, "tags", None)
    if tags is not None and hasattr(tags, "getall"):
        pictures += [(frame.type, frame.data) for frame in tags.getall("APIC")]
    return pictures


def folder_image(folder):
    candidates = sorted(path for path in folder.iterdir() if path.is_file())
    for stem in FOLDER_IMAGE_STEMS:
        for path in candidates:
            if path.stem.lower() == stem and path.suffix.lower() in FOLDER_IMAGE_SUFFIXES:
                return path.read_bytes()
    return None


def find_art(root, relative_paths):
    fallback = None
    for relative in relative_paths:
        for picture_type, data in embedded_pictures(root / relative):
            if picture_type == FRONT_COVER:
                return data
            fallback = data if fallback is None else fallback
    if fallback is not None:
        return fallback
    for folder in dict.fromkeys((root / relative).parent for relative in relative_paths):
        data = folder_image(folder)
        if data is not None:
            return data
    return None


def album_sources(model):
    sources = []
    for album in model.albums:
        artist = model.album_artists[album.album_artist].name
        sources.append(AlbumSource(album_key(artist, album.title), album.title, artist))
    for track, album_index in zip(model.tracks, model.track_albums):
        sources[album_index].paths.append(track.path)
    return sources


def pack_pixels(pixels):
    return struct.pack(f"<{len(pixels)}H", *pixels)


def encode_covers(images):
    keys = sorted(images)[:MAX_COVERS]
    offset = HEADER.size + ENTRY.size * len(keys)
    header = HEADER.pack(MAGIC, VERSION, len(keys), FULL_SIZE, HALF_SIZE, 0)
    index, records = bytearray(), bytearray()
    for key in keys:
        image = images[key]
        record = pack_pixels(cover_pixels(image, FULL_SIZE)) + pack_pixels(
            cover_pixels(image, HALF_SIZE))
        index += ENTRY.pack(key, offset + len(records))
        records += record
    return header + bytes(index) + bytes(records)


def load_image(data):
    from PIL import Image
    image = Image.open(io.BytesIO(data))
    image.load()
    return image


def collect_images(root, sources):
    images, missing = {}, []
    for source in sources:
        if source.key in images:
            continue
        data = find_art(root, source.paths)
        try:
            image = None if data is None else load_image(data)
        except Exception:
            image = None
        if image is None:
            missing.append(f"{source.artist} - {source.title}")
        else:
            images[source.key] = image
    return images, missing


def parse_arguments():
    parser = argparse.ArgumentParser(description="Export album covers for the Echo Mini")
    parser.add_argument("--music", type=Path, required=True, help="music root (read only)")
    parser.add_argument("--output", type=Path, required=True, help="covers.bin to write")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    root = arguments.music.resolve()
    tracks, playlists, failures = library_index.scan(root)
    for failure in failures:
        print(f"skipped {failure}", file=sys.stderr)
    sources = album_sources(library_index.build_library(tracks, playlists))
    images, missing = collect_images(root, sources)
    for album in missing:
        print(f"no cover: {album}", file=sys.stderr)
    data = encode_covers(images)
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_bytes(data)
    print(f"{len(images)} covers, {len(missing)} albums without art, "
          f"{len(data):,} bytes -> {arguments.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

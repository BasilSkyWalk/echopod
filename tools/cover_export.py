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
OTHER_PICTURE = 0
APE_COVER_PREFIX = "cover art ("
APE_FRONT_KEY = "cover art (front)"
ASF_PICTURE_HEAD = struct.Struct("<BI")
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


def mp4_pictures(tags):
    return [(FRONT_COVER, bytes(cover)) for cover in tags.get("covr", [])]


def ape_picture(key, data):
    name_end = data.find(b"\x00")
    if name_end < 0:
        return None
    kind = FRONT_COVER if key.lower() == APE_FRONT_KEY else OTHER_PICTURE
    return kind, data[name_end + 1:]


def ape_pictures(tags):
    from mutagen.apev2 import BINARY
    pictures = []
    for key, value in tags.items():
        if (len(key) > len(APE_COVER_PREFIX) and key.lower().startswith(APE_COVER_PREFIX)
                and value.kind == BINARY):
            picture = ape_picture(key, value.value)
            if picture is not None:
                pictures.append(picture)
    return pictures


def skip_utf16_string(data, cursor):
    while cursor + 2 <= len(data):
        unit = data[cursor:cursor + 2]
        cursor += 2
        if unit == b"\x00\x00":
            break
    return cursor


def asf_picture(data):
    if len(data) < ASF_PICTURE_HEAD.size:
        return None
    kind, length = ASF_PICTURE_HEAD.unpack_from(data)
    cursor = skip_utf16_string(data, skip_utf16_string(data, ASF_PICTURE_HEAD.size))
    if cursor > len(data) or length > len(data) - cursor:
        return None
    return kind, data[cursor:cursor + length]


def asf_pictures(tags):
    pictures = []
    for attribute in tags.get("WM/Picture", []):
        picture = asf_picture(bytes(attribute.value)) if isinstance(attribute.value, bytes) else None
        if picture is not None:
            pictures.append(picture)
    return pictures


def tag_pictures(tags):
    from mutagen.apev2 import APEv2
    from mutagen.asf import ASFTags
    from mutagen.mp4 import MP4Tags
    if isinstance(tags, MP4Tags):
        return mp4_pictures(tags)
    if isinstance(tags, APEv2):
        return ape_pictures(tags)
    if isinstance(tags, ASFTags):
        return asf_pictures(tags)
    if hasattr(tags, "getall"):
        return [(frame.type, frame.data) for frame in tags.getall("APIC")]
    return []


def embedded_pictures(path):
    from mutagen import File, MutagenError
    try:
        audio = File(path)
    except (MutagenError, OSError):
        return []
    if audio is None:
        return []
    pictures = [(picture.type, picture.data) for picture in getattr(audio, "pictures", [])]
    tags = getattr(audio, "tags", None)
    if tags is not None:
        pictures += tag_pictures(tags)
    return pictures


def folder_image(folder):
    try:
        candidates = sorted(path for path in folder.iterdir() if path.is_file())
    except OSError:
        return None
    for stem in FOLDER_IMAGE_STEMS:
        for path in candidates:
            if path.stem.lower() == stem and path.suffix.lower() in FOLDER_IMAGE_SUFFIXES:
                return path.read_bytes()
    return None


def find_art(root, relative_paths):
    if not relative_paths:
        return None
    pictures = embedded_pictures(root / relative_paths[0])
    for picture_type, data in pictures:
        if picture_type == FRONT_COVER:
            return data
    if pictures:
        return pictures[0][1]
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


def string_at(data, strings_offset, offset):
    start = strings_offset + offset
    return data[start:data.index(b"\x00", start)].decode("utf-8")


def section(data, index):
    entry = library_index.SECTION_ENTRY.unpack_from(data, 8 + index * library_index.SECTION_ENTRY.size)
    return entry[0], entry[1]


def album_sources_from_library(data):
    if data[:4] != b"EMDB":
        raise ValueError("not an Echo Mini library")
    tracks_offset, track_count = section(data, 0)
    albums_offset, album_count = section(data, 1)
    album_artists_offset, _ = section(data, 3)
    strings_offset, _ = section(data, 9)
    paths = [string_at(data, strings_offset,
                       library_index.TRACK_RECORD.unpack_from(
                           data, tracks_offset + index * library_index.TRACK_RECORD.size)[1])
             for index in range(track_count)]
    sources = []
    for index in range(album_count):
        title, album_artist, _, _, first, count, _ = library_index.ALBUM_RECORD.unpack_from(
            data, albums_offset + index * library_index.ALBUM_RECORD.size)
        name = library_index.GROUP_RECORD.unpack_from(
            data, album_artists_offset + album_artist * library_index.GROUP_RECORD.size)[0]
        artist = string_at(data, strings_offset, name)
        album = string_at(data, strings_offset, title)
        sources.append(AlbumSource(album_key(artist, album), album, artist, paths[first:first + count]))
    return sources


def pack_pixels(pixels):
    return struct.pack(f"<{len(pixels)}H", *pixels)


def cover_record(image):
    return pack_pixels(cover_pixels(image, FULL_SIZE)) + pack_pixels(cover_pixels(image, HALF_SIZE))


def encode_records(records):
    keys = sorted(records)[:MAX_COVERS]
    offset = HEADER.size + ENTRY.size * len(keys)
    header = HEADER.pack(MAGIC, VERSION, len(keys), FULL_SIZE, HALF_SIZE, 0)
    index, data = bytearray(), bytearray()
    for key in keys:
        index += ENTRY.pack(key, offset + len(data))
        data += records[key]
    return header + bytes(index) + bytes(data)


def encode_covers(images):
    return encode_records({key: cover_record(images[key]) for key in sorted(images)[:MAX_COVERS]})


def load_image(data):
    from PIL import Image
    image = Image.open(io.BytesIO(data))
    image.load()
    return image


def unchanged(image):
    return image


def collect_images(root, sources, convert=unchanged):
    images, missing = {}, []
    for source in sources:
        if source.key in images:
            continue
        data = find_art(root, source.paths)
        try:
            image = None if data is None else convert(load_image(data))
        except Exception:
            image = None
        if image is None:
            missing.append(f"{source.artist} - {source.title}")
        else:
            images[source.key] = image
    return images, missing


def parse_arguments(argv):
    parser = argparse.ArgumentParser(description="Export album covers for the Echo Mini")
    parser.add_argument("--music", type=Path, required=True, help="music root (read only)")
    parser.add_argument("--library", type=Path,
                        help="an existing library.db for this music root, to skip the tag scan")
    parser.add_argument("--output", type=Path, required=True, help="covers.bin to write")
    return parser.parse_args(argv)


def scanned_sources(root):
    tracks, playlists, failures = library_index.scan(root)
    for failure in failures:
        print(f"skipped {failure}", file=sys.stderr)
    return album_sources(library_index.build_library(tracks, playlists))


def main(argv=None):
    arguments = parse_arguments(argv)
    root = arguments.music.resolve()
    if arguments.library is not None:
        sources = album_sources_from_library(arguments.library.read_bytes())
    else:
        sources = scanned_sources(root)
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

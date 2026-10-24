import argparse
import io
import random
import sys
from pathlib import Path

FRAME_HEADER = bytes([0xFF, 0xFB, 0x90, 0xC0])
FRAME_SIZE = 417
FRAME_COUNT = 12
TRACKS_PER_ALBUM = 12
ALBUMS_PER_ARTIST = 4
COVER_SIZE = 64
FRONT_COVER = 3
WORDS = ["Love", "Night", "Blue", "Fire", "Dream", "Heart", "Road", "Light", "Rain", "Gold",
         "River", "Storm", "Ocean", "Shadow", "City", "Dance", "Echo", "Wild", "Stone", "Glass",
         "Été", "Café", "Ångel", "Mötley", "Naïve", "Smörgås"]
GENRES = ["Rock", "Pop", "Jazz", "Electronic", "Metal", "Classical", "Hip-Hop", "Folk",
          "Ambient", "Blues", "Soundtrack", "Indie"]


def syncsafe(value):
    return bytes([(value >> 21) & 0x7F, (value >> 14) & 0x7F, (value >> 7) & 0x7F, value & 0x7F])


def frame(frame_id, data):
    return frame_id.encode("ascii") + len(data).to_bytes(4, "big") + b"\x00\x00" + data


def text_frame(frame_id, value):
    return frame(frame_id, b"\x01\xff\xfe" + value.encode("utf-16-le"))


def picture_frame(jpeg):
    return frame("APIC", b"\x00image/jpeg\x00" + bytes([FRONT_COVER]) + b"\x00" + jpeg)


def id3_tag(frames):
    body = b"".join(frames)
    return b"ID3\x03\x00\x00" + syncsafe(len(body)) + body


def audio_frames():
    audio = FRAME_HEADER + bytes(FRAME_SIZE - len(FRAME_HEADER))
    return audio * FRAME_COUNT


def words(generator, count):
    return " ".join(generator.choice(WORDS) for _ in range(count))


def safe(name):
    return "".join("_" if letter in '\\/:*?"<>|' else letter for letter in name)


def plan_tracks(count, seed, tracks_per_album=TRACKS_PER_ALBUM):
    generator = random.Random(seed)
    tracks = []
    artist = 0
    album_index = 0
    while len(tracks) < count:
        artist_name = f"{words(generator, 1 + generator.randrange(2))} {artist:04d}"
        for album in range(ALBUMS_PER_ARTIST):
            album_name = f"{words(generator, 1 + generator.randrange(3))} {album + 1}"
            year = 1960 + generator.randrange(66)
            genre = generator.choice(GENRES)
            for number in range(1, tracks_per_album + 1):
                if len(tracks) == count:
                    return tracks
                tracks.append({
                    "index": len(tracks),
                    "album_index": album_index,
                    "artist": artist_name,
                    "album": album_name,
                    "title": f"{words(generator, 1 + generator.randrange(4))}",
                    "number": number,
                    "year": year,
                    "genre": genre,
                })
            album_index += 1
        artist += 1
    return tracks


def cover_jpeg(track):
    from PIL import Image, ImageDraw

    generator = random.Random(track["album_index"])
    background = tuple(64 + generator.randrange(160) for _ in range(3))
    image = Image.new("RGB", (COVER_SIZE, COVER_SIZE), background)
    ImageDraw.Draw(image).text((6, 24), str(track["album_index"] + 1), fill=(255, 255, 255))
    output = io.BytesIO()
    image.save(output, "JPEG", quality=85)
    return output.getvalue()


def track_bytes(track, covers=False):
    frames = [
        text_frame("TIT2", track["title"]),
        text_frame("TPE1", track["artist"]),
        text_frame("TPE2", track["artist"]),
        text_frame("TALB", track["album"]),
        text_frame("TCON", track["genre"]),
        text_frame("TYER", str(track["year"])),
        text_frame("TRCK", str(track["number"])),
        text_frame("TPOS", "1"),
    ]
    if covers:
        frames.append(picture_frame(cover_jpeg(track)))
    return id3_tag(frames) + audio_frames()


def track_path(root, track, flat=False):
    if flat:
        return root / f"{track['index'] + 1:05d} - {safe(track['title'])}.mp3"
    name = f"{track['number']:02d} - {safe(track['title'])}.mp3"
    return root / safe(track["artist"]) / safe(track["album"]) / name


def write_library(root, count, seed, tracks_per_album=TRACKS_PER_ALBUM, flat=False,
                  covers=False):
    total = 0
    for track in plan_tracks(count, seed, tracks_per_album):
        path = track_path(root, track, flat)
        path.parent.mkdir(parents=True, exist_ok=True)
        data = track_bytes(track, covers)
        path.write_bytes(data)
        total += len(data)
    return total


def parse_arguments(argv):
    parser = argparse.ArgumentParser(
        description="Write a folder of tiny tagged MP3 files for testing big libraries")
    parser.add_argument("--output", type=Path, required=True, help="folder to create")
    parser.add_argument("--tracks", type=int, required=True)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--tracks-per-album", type=int, default=TRACKS_PER_ALBUM)
    parser.add_argument("--flat", action="store_true", help="put every track in one folder")
    parser.add_argument("--covers", action="store_true",
                        help="embed a small front cover, different for each album")
    return parser.parse_args(argv)


def main(argv=None):
    arguments = parse_arguments(argv)
    total = write_library(arguments.output, arguments.tracks, arguments.seed,
                          arguments.tracks_per_album, arguments.flat, arguments.covers)
    print(f"wrote {arguments.tracks} tracks, {total} bytes, to {arguments.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

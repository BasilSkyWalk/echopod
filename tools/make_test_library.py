import argparse
import random
import sys
from pathlib import Path

FRAME_HEADER = bytes([0xFF, 0xFB, 0x90, 0xC0])
FRAME_SIZE = 417
FRAME_COUNT = 12
TRACKS_PER_ALBUM = 12
ALBUMS_PER_ARTIST = 4
WORDS = ["Love", "Night", "Blue", "Fire", "Dream", "Heart", "Road", "Light", "Rain", "Gold",
         "River", "Storm", "Ocean", "Shadow", "City", "Dance", "Echo", "Wild", "Stone", "Glass",
         "Été", "Café", "Ångel", "Mötley", "Naïve", "Smörgås"]
GENRES = ["Rock", "Pop", "Jazz", "Electronic", "Metal", "Classical", "Hip-Hop", "Folk",
          "Ambient", "Blues", "Soundtrack", "Indie"]


def syncsafe(value):
    return bytes([(value >> 21) & 0x7F, (value >> 14) & 0x7F, (value >> 7) & 0x7F, value & 0x7F])


def text_frame(frame_id, value):
    data = b"\x01\xff\xfe" + value.encode("utf-16-le")
    return frame_id.encode("ascii") + len(data).to_bytes(4, "big") + b"\x00\x00" + data


def id3_tag(fields):
    frames = b"".join(text_frame(frame_id, value) for frame_id, value in fields)
    return b"ID3\x03\x00\x00" + syncsafe(len(frames)) + frames


def audio_frames():
    frame = FRAME_HEADER + bytes(FRAME_SIZE - len(FRAME_HEADER))
    return frame * FRAME_COUNT


def words(generator, count):
    return " ".join(generator.choice(WORDS) for _ in range(count))


def safe(name):
    return "".join("_" if letter in '\\/:*?"<>|' else letter for letter in name)


def plan_tracks(count, seed):
    generator = random.Random(seed)
    tracks = []
    artist = 0
    while len(tracks) < count:
        artist_name = f"{words(generator, 1 + generator.randrange(2))} {artist:04d}"
        for album in range(ALBUMS_PER_ARTIST):
            album_name = f"{words(generator, 1 + generator.randrange(3))} {album + 1}"
            year = 1960 + generator.randrange(66)
            genre = generator.choice(GENRES)
            for number in range(1, TRACKS_PER_ALBUM + 1):
                if len(tracks) == count:
                    return tracks
                tracks.append({
                    "artist": artist_name,
                    "album": album_name,
                    "title": f"{words(generator, 1 + generator.randrange(4))}",
                    "number": number,
                    "year": year,
                    "genre": genre,
                })
        artist += 1
    return tracks


def track_bytes(track):
    tag = id3_tag([
        ("TIT2", track["title"]),
        ("TPE1", track["artist"]),
        ("TPE2", track["artist"]),
        ("TALB", track["album"]),
        ("TCON", track["genre"]),
        ("TYER", str(track["year"])),
        ("TRCK", str(track["number"])),
        ("TPOS", "1"),
    ])
    return tag + audio_frames()


def track_path(root, track):
    name = f"{track['number']:02d} - {safe(track['title'])}.mp3"
    return root / safe(track["artist"]) / safe(track["album"]) / name


def write_library(root, count, seed):
    total = 0
    for track in plan_tracks(count, seed):
        path = track_path(root, track)
        path.parent.mkdir(parents=True, exist_ok=True)
        data = track_bytes(track)
        path.write_bytes(data)
        total += len(data)
    return total


def parse_arguments(argv):
    parser = argparse.ArgumentParser(
        description="Write a folder of tiny tagged MP3 files for testing big libraries")
    parser.add_argument("--output", type=Path, required=True, help="folder to create")
    parser.add_argument("--tracks", type=int, required=True)
    parser.add_argument("--seed", type=int, default=1)
    return parser.parse_args(argv)


def main(argv=None):
    arguments = parse_arguments(argv)
    total = write_library(arguments.output, arguments.tracks, arguments.seed)
    print(f"wrote {arguments.tracks} tracks, {total} bytes, to {arguments.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

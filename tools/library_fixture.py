import argparse
import sys
from pathlib import Path

from library_index import RawPlaylist, TrackInfo, build_library, encode_library

FIXTURE_DURATION_MS = 60000


def make_track(path, title, artist, album_artist, album, genre, year, disc, number):
    return TrackInfo(path=path, title=title, artist=artist, album_artist=album_artist,
                     album=album, genre=genre, year=year, disc=disc, number=number,
                     duration_ms=FIXTURE_DURATION_MS)


FIXTURE_TRACKS = [
    make_track("AC DC/Back In Black/1-02 - Shoot to Thrill.flac", "Shoot to Thrill", "AC/DC",
               "AC/DC", "Back In Black", "Metal", 1980, 1, 2),
    make_track("AC DC/Back In Black/1-01 - Hells Bells.flac", "Hells Bells", "AC/DC", "AC/DC",
               "Back In Black", "Metal", 1980, 1, 1),
    make_track("AC DC/High Voltage/1-01 - Long Way.flac", "It's a Long Way to the Top", "AC/DC",
               "AC/DC", "High Voltage", "Metal", 1976, 1, 1),
    make_track("Björk/Homogenic/02 - Jóga.flac", "Jóga", "Björk", "Björk", "Homogenic",
               "Electronic", 1997, 1, 2),
    make_track("Björk/Homogenic/01 - Hunter.flac", "Hunter", "Björk", "Björk", "Homogenic",
               "Electronic", 1997, 1, 1),
    make_track("Various Artists/The Best of Something/01 - Help!.flac", "Help!", "The Beatles",
               "Various Artists", "The Best of Something", "Pop", 2001, 1, 1),
    make_track("Various Artists/The Best of Something/02 - Army of Me.flac", "Army of Me",
               "Björk", "Various Artists", "The Best of Something", "Pop", 2001, 1, 2),
    make_track("acloudyskye/This Won't Be The Last Time/01 - Shoots.flac", "Shoots",
               "acloudyskye", "acloudyskye", "This Won't Be The Last Time", "Unknown Genre",
               2025, 1, 1),
]


FIXTURE_PLAYLISTS = [
    RawPlaylist("Road Trip", "Playlists", [
        "../acloudyskye/This Won't Be The Last Time/01 - Shoots.flac",
        "../AC DC/Back In Black/1-01 - Hells Bells.flac",
        "../Missing/Gone.flac",
        "../Björk/Homogenic/02 - Jóga.flac",
    ]),
]


def fixture_bytes():
    return encode_library(build_library(FIXTURE_TRACKS, FIXTURE_PLAYLISTS))


def parse_arguments():
    parser = argparse.ArgumentParser(description="Write the small library used by the C tests")
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

import argparse
import random
import sys
from pathlib import Path

from library_index import RawPlaylist, TrackInfo, build_library, encode_library

SEED = 20261018
TRACK_COUNT = 400
ARTISTS = ["The Beatles", "Beatles", "the beatles", "Björk", "Bjork", "AC/DC", "Ác/Dc", "Sigur Rós",
           "Mötley Crüe", "Tiësto", "ABBA", "Abba", "Zebra", "Ñandú", "Œuvre", "Straße", "Strasse",
           "  Spaced  ", "The The", "Théâtre", "Ágætis", "ΣΊΣΥΦΟΣ", "Кино", "エンジェル",
           "Various Artists"]
ALBUMS = ["Homogenic", "homogenic", "Back In Black", "The Best Of", "Best Of", "Ágætis byrjun",
          "Agaetis Byrjun", "Live", "LIVE", "1999", "Greatest Hits", "The Wall", "Wall"]
GENRES = ["Rock", "rock", "Pop", "Électro", "Electro", "Unknown Genre", "Metal", "Jazz"]
TITLES = ["Intro", "intro", "Outro", "The Song", "Song", "Help!", "Jóga", "Joga", "Hunter",
          "Army of Me", "Ünïcödé", "Track", "Ballad", "Ballad", "Zzz", "Aaa", "1999", "½ Life"]
FOLDERS = ["Music", "Other", "Incoming", "Ac DC", "ac dc", "Ärzte", "Arzte", "Disc 1", "Disc 2",
           "CD1", "cd1", "Z", "a"]


def random_path(rng, index):
    depth = rng.choice([0, 1, 2, 2, 3, 3, 3, 4])
    parts = [rng.choice(FOLDERS) for _ in range(depth)]
    name = rng.choice([f"{rng.randint(1, 20):02d} - {rng.choice(TITLES)}",
                       f"{rng.randint(1, 3)}-{rng.randint(1, 20):02d} - {rng.choice(TITLES)}",
                       rng.choice(TITLES)])
    return "/".join(parts + [f"{name} {index}.flac"])


def random_track(rng, index):
    artist = rng.choice(ARTISTS)
    return TrackInfo(path=random_path(rng, index), title=rng.choice(TITLES), artist=artist,
                     album_artist=rng.choice([artist, artist, "Various Artists", rng.choice(ARTISTS)]),
                     album=rng.choice(ALBUMS), genre=rng.choice(GENRES),
                     year=rng.choice([0, 0, 1976, 1980, 1997, 1999, 2001, 2025]),
                     disc=rng.choice([1, 1, 1, 2, 3]), number=rng.randint(0, 25),
                     duration_ms=rng.randint(1000, 600000))


PLAYLIST_COUNT = 30
PLAYLIST_NAMES = ["Road Trip", "road trip", "The Gym", "Gym", "Ünïcödé", "Chill", "Chill",
                  "Best Of", "Ágætis"]


def tracks():
    rng = random.Random(SEED)
    return [random_track(rng, index) for index in range(TRACK_COUNT)]


def entry_for(rng, path, folder):
    parts = path.split("/")
    style = rng.randrange(9)
    if style == 0:
        return path
    if style == 1:
        return path.upper() if rng.random() < 0.5 else path.lower()
    if style == 2:
        return "\\".join(parts)
    if style == 3:
        return "D:\\Music\\" + "\\".join(parts)
    if style == 4:
        return parts[-1]
    if style == 5:
        depth = len(folder.split("/")) if folder else 0
        return "../" * depth + path
    if style == 6:
        return "./" + path
    if style == 7:
        return "Missing/" + parts[-1] + ".gone"
    return "http://example.com/" + parts[-1]


def playlists(track_list):
    rng = random.Random(SEED + 1)
    result = []
    for _ in range(PLAYLIST_COUNT):
        folder = rng.choice(["", "Playlists", "Music/Lists", "a"])
        entries = [entry_for(rng, rng.choice(track_list).path, folder)
                   for _ in range(rng.randint(0, 25))]
        result.append(RawPlaylist(rng.choice(PLAYLIST_NAMES), folder, entries))
    return result


def playlist_rows(raw_playlists):
    rows = []
    for playlist in raw_playlists:
        rows.append("\t".join(["#PLAYLIST", playlist.name, playlist.folder]))
        rows.extend("\t" + entry for entry in playlist.entries)
    return rows


def tsv(track_list, raw_playlists=()):
    rows = []
    for track in track_list:
        rows.append("\t".join([track.path, track.title, track.artist, track.album_artist,
                               track.album, track.genre, str(track.year), str(track.disc),
                               str(track.number), str(track.duration_ms)]))
    rows += playlist_rows(raw_playlists)
    return ("\n".join(rows) + "\n").encode("utf-8")


def outputs(directory):
    track_list = tracks()
    raw_playlists = playlists(track_list)
    return {directory / "tracks.tsv": tsv(track_list),
            directory / "library.db": encode_library(build_library(track_list)),
            directory / "with_playlists.tsv": tsv(track_list, raw_playlists),
            directory / "with_playlists.db": encode_library(build_library(track_list, raw_playlists))}


def main():
    parser = argparse.ArgumentParser(description="Write the random builder fixture")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    files = outputs(arguments.output)
    if arguments.check:
        stale = [path for path, data in files.items()
                 if not path.exists() or path.read_bytes() != data]
        for path in stale:
            print(f"out of date: {path}")
        return 1 if stale else 0
    arguments.output.mkdir(parents=True, exist_ok=True)
    for path, data in files.items():
        path.write_bytes(data)
    return 0


if __name__ == "__main__":
    sys.exit(main())

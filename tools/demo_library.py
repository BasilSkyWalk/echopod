import argparse
import math
import sys
from pathlib import Path

import cover_export
import library_index

COVER_SIZE = 240

ALBUMS = (
    ("Paper Lanterns", "Slow Tide", 2019, "Indie", "rings", ((24, 60, 92), (240, 196, 120)),
     ("Harbour Lights", "Slow Tide", "Kite String", "Seven Bridges", "Low Sun", "Salt and Ember",
      "Quiet Engines")),
    ("Paper Lanterns", "Glass Harbour", 2022, "Indie", "stripes", ((20, 84, 88), (236, 232, 208)),
     ("First Ferry", "Glass Harbour", "Weathervane", "Nine Lamps", "Tidewrack", "Home by Morning")),
    ("Northern Static", "Signal Fires", 2016, "Electronic", "grid", ((36, 20, 72), (96, 232, 208)),
     ("Carrier Wave", "Signal Fires", "Blue Shift", "Halogen", "Night Relay", "Zero Crossing",
      "Afterimage", "Last Transmission")),
    ("The Copper Owls", "Lowlands", 2011, "Folk", "hills", ((72, 44, 24), (232, 180, 96)),
     ("Marsh Road", "Lowlands", "Barley Moon", "The Miller's Reel", "Stone Fences", "Come Evening")),
    ("Juno Vale", "Afterglow Motel", 2021, "Pop", "sun", ((200, 56, 96), (252, 208, 96)),
     ("Vacancy", "Afterglow Motel", "Neon Pool", "Postcards", "Room Eleven", "Check Out Time",
      "Long Way Round")),
    ("Halden Trio", "Blue Hours", 1998, "Jazz", "diagonal", ((16, 28, 64), (120, 168, 232)),
     ("Blue Hours", "Third Street", "Slow Brush", "A Walk at Two", "Small Rooms", "Closing Set")),
    ("Vast Meridian", "Orbits", 2024, "Ambient", "rings", ((12, 16, 28), (168, 144, 232)),
     ("Perihelion", "Slow Orbit", "Dust Lanes", "Far Side", "Aphelion")),
    ("Saltwater Radio", "Tape Season", 2008, "Rock", "stripes", ((40, 40, 44), (232, 88, 56)),
     ("Rewind", "Tape Season", "Dial Tone Summer", "Static on the Pier", "Boardwalk", "Hiss",
      "Side B", "Fade Out")),
)


def seconds_of(album, track):
    return 160 + (album * 53 + track * 37) % 210


def mixed(first, second, share):
    return tuple(round(a + (b - a) * share) for a, b in zip(first, second))


def share_at(pattern, x, y):
    across, down = x / (COVER_SIZE - 1), y / (COVER_SIZE - 1)
    if pattern == "rings":
        return 0.5 + 0.5 * math.cos(math.hypot(across - 0.5, down - 0.62) * 26)
    if pattern == "stripes":
        return 1.0 if int((across + down * 0.6) * 9) % 2 else down * 0.35
    if pattern == "grid":
        return 1.0 if x % 30 < 3 or y % 30 < 3 else down * 0.5
    if pattern == "hills":
        return 1.0 if down > 0.55 + 0.12 * math.sin(across * 9) else down * 0.5
    if pattern == "sun":
        return 1.0 if math.hypot(across - 0.5, down - 0.7) < 0.3 else down * 0.6
    return (across + down) / 2


def cover_image(pattern, colors):
    from PIL import Image
    image = Image.new("RGB", (COVER_SIZE, COVER_SIZE))
    image.putdata([mixed(colors[0], colors[1], share_at(pattern, x, y))
                   for y in range(COVER_SIZE) for x in range(COVER_SIZE)])
    return image


def tracks_and_covers():
    tracks, covers = [], {}
    for number, (artist, album, year, genre, pattern, colors, titles) in enumerate(ALBUMS):
        covers[cover_export.album_key(artist, album)] = cover_image(pattern, colors)
        for index, title in enumerate(titles, 1):
            tracks.append(library_index.TrackInfo(
                path=f"{artist}/{album}/{index:02d} {title}.flac", title=title, artist=artist,
                album_artist=artist, album=album, genre=genre, year=year, disc=1, number=index,
                duration_ms=seconds_of(number, index) * 1000))
    return tracks, covers


def build(output):
    tracks, covers = tracks_and_covers()
    output.mkdir(parents=True, exist_ok=True)
    library = library_index.encode_library(library_index.build_library(tracks, []))
    (output / "library.db").write_bytes(library)
    (output / "covers.bin").write_bytes(cover_export.encode_covers(covers))


def main(argv=None):
    parser = argparse.ArgumentParser(description="Write the invented library the theme editor shows")
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args(argv)
    try:
        build(arguments.output)
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

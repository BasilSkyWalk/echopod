import sys
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIRECTORY))

import library_index

FIXTURES = TOOLS_DIRECTORY.parent / "tests" / "fixtures" / "tags"

FULL = {"title": "Svefn-g-englar ✓", "artist": "Sigur Rós", "album_artist": "Sigur Rós & Friends",
        "album": "Ágætis byrjun", "genre": "Post-Rock", "year": 1999, "disc": 1, "number": 4}

EXPECTED = {
    "tagged.flac": dict(FULL, duration_ms=1000),
    "tagged.ogg": dict(FULL, duration_ms=1001),
    "id3v24.mp3": dict(FULL, duration_ms=1045),
    "id3v23_vbr.mp3": dict(FULL, genre="Rock", duration_ms=1032),
    "aac.m4a": dict(FULL, duration_ms=1023),
    "alac.m4a": dict(FULL, duration_ms=1000),
    "tagged.wav": dict(FULL, duration_ms=1000),
    "tagged.wma": dict(FULL, duration_ms=1000),
    "tagged.ape": dict(FULL, duration_ms=3442),
    "tagged.dsf": dict(FULL, duration_ms=100),
    "tagged.dff": dict(FULL, duration_ms=100),
    "id3v1_no_xing.mp3": {"title": "Old Song", "artist": "Old Artist", "album_artist": "Old Artist",
                          "album": "Old Album", "genre": "Rock", "year": 1987, "disc": 1,
                          "number": 7, "duration_ms": 947},
    "untagged.flac": {"title": "untagged", "artist": "Unknown Artist",
                      "album_artist": "Unknown Artist", "album": "tags", "genre": "Unknown Genre",
                      "year": 0, "disc": 1, "number": 0, "duration_ms": 1000},
}


class TrackTagsTest(unittest.TestCase):
    def test_every_stock_format_is_listed(self):
        for suffix in (".flac", ".ogg", ".mp3", ".m4a", ".wav", ".wma", ".ape", ".dsf", ".dff"):
            self.assertIn(suffix, library_index.AUDIO_SUFFIXES)

    def test_each_format_reads_its_tags_and_duration(self):
        for name, expected in EXPECTED.items():
            with self.subTest(name=name):
                track = library_index.read_track(FIXTURES.parent, FIXTURES / name)
                self.assertEqual(f"tags/{name}", track.path)
                for field_name, value in expected.items():
                    self.assertEqual(value, getattr(track, field_name), field_name)


if __name__ == "__main__":
    unittest.main()

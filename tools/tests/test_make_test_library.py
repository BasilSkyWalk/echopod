import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIRECTORY))

import make_test_library


def tag_size(data):
    size = data[6:10]
    return size[0] << 21 | size[1] << 14 | size[2] << 7 | size[3]


class TestLibraryTest(unittest.TestCase):
    def test_each_track_is_an_id3v23_tag_then_back_to_back_mpeg_frames(self):
        track = make_test_library.plan_tracks(1, 3)[0]
        data = make_test_library.track_bytes(track)
        self.assertEqual(b"ID3\x03\x00", data[:5])
        start = 10 + tag_size(data)
        for frame in range(make_test_library.FRAME_COUNT):
            offset = start + frame * make_test_library.FRAME_SIZE
            self.assertEqual(make_test_library.FRAME_HEADER, data[offset:offset + 4])
        self.assertEqual(start + make_test_library.FRAME_COUNT * make_test_library.FRAME_SIZE,
                         len(data))

    def test_tags_are_utf16_with_a_byte_order_mark(self):
        frame = make_test_library.text_frame("TIT2", "Café")
        self.assertEqual(b"TIT2", frame[:4])
        self.assertEqual(len(frame) - 10, int.from_bytes(frame[4:8], "big"))
        self.assertEqual(b"\x01\xff\xfe" + "Café".encode("utf-16-le"), frame[10:])

    def test_the_same_seed_writes_the_same_library(self):
        self.assertEqual(make_test_library.plan_tracks(30, 7), make_test_library.plan_tracks(30, 7))
        self.assertNotEqual(make_test_library.plan_tracks(30, 7),
                            make_test_library.plan_tracks(30, 8))

    def test_the_folder_holds_the_requested_number_of_tracks(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "Test Library"
            make_test_library.write_library(root, 30, 1)
            self.assertEqual(30, len(list(root.rglob("*.mp3"))))

    def test_albums_hold_twelve_tracks_and_artists_four_albums(self):
        tracks = make_test_library.plan_tracks(100, 1)
        albums = {(track["artist"], track["album"]) for track in tracks}
        artists = {track["artist"] for track in tracks}
        self.assertEqual(9, len(albums))
        self.assertEqual(3, len(artists))


    def test_albums_can_hold_any_number_of_tracks(self):
        tracks = make_test_library.plan_tracks(10, 1, tracks_per_album=2)
        albums = {(track["artist"], track["album"]) for track in tracks}
        self.assertEqual(5, len(albums))
        self.assertEqual([1, 2], sorted({track["number"] for track in tracks}))

    def test_a_flat_library_keeps_every_track_in_one_folder(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "Test Library"
            make_test_library.write_library(root, 40, 1, tracks_per_album=2, flat=True)
            files = list(root.iterdir())
            self.assertEqual(40, len(files))
            self.assertTrue(all(path.is_file() for path in files))

    def test_each_album_gets_its_own_embedded_front_cover(self):
        tracks = make_test_library.plan_tracks(4, 1, tracks_per_album=2)
        first = make_test_library.track_bytes(tracks[0], covers=True)
        second = make_test_library.track_bytes(tracks[2], covers=True)
        start = first.index(b"APIC")
        size = int.from_bytes(first[start + 4:start + 8], "big")
        body = first[start + 10:start + 10 + size]
        self.assertEqual(b"\x00image/jpeg\x00\x03\x00", body[:14])
        self.assertEqual(b"\xff\xd8", body[14:16])
        self.assertLessEqual(start + 10 + size, 10 + tag_size(first))
        self.assertNotEqual(make_test_library.cover_jpeg(tracks[0]),
                            make_test_library.cover_jpeg(tracks[2]))
        self.assertIn(b"APIC", second)

    def test_tracks_have_no_cover_unless_asked(self):
        track = make_test_library.plan_tracks(1, 1)[0]
        self.assertNotIn(b"APIC", make_test_library.track_bytes(track))


if __name__ == "__main__":
    unittest.main()

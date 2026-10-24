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


if __name__ == "__main__":
    unittest.main()

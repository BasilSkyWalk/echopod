import io
import sys
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIRECTORY))

import library_index
import tag_skim

TAGS = TOOLS_DIRECTORY.parent / "tests" / "fixtures" / "tags"


def track_of(audio):
    duration = audio.info.length if audio is not None else 0.0
    return library_index.track_from_tags("x", library_index.normalized_tags(audio), duration)


def synchsafe_bytes(value):
    return bytes([(value >> 21) & 0x7F, (value >> 14) & 0x7F, (value >> 7) & 0x7F, value & 0x7F])


class PictureRangeTest(unittest.TestCase):
    def test_only_the_image_bytes_of_a_flac_picture_are_skipped(self):
        with open(TAGS / "tagged.flac", "rb") as handle:
            ranges = tag_skim.picture_ranges(handle)
            self.assertIn((292, 926), ranges)
            handle.seek(292)
            self.assertEqual(b"\xff\xd8", handle.read(2))

    def test_flac_padding_is_skipped_whole(self):
        streaminfo = bytes([0x00, 0, 0, 34]) + bytes(34)
        padding = bytes([0x81, 0, 0, 100]) + bytes(100)
        data = b"fLaC" + streaminfo + padding
        self.assertEqual([(46, 146)], tag_skim.picture_ranges(io.BytesIO(data)))

    def test_id3_picture_frames_are_skipped(self):
        with open(TAGS / "id3v24.mp3", "rb") as handle:
            self.assertEqual([(226, 874)], tag_skim.picture_ranges(handle))

    def test_an_unsynchronised_id3_tag_is_read_whole(self):
        frame = b"APIC" + synchsafe_bytes(4) + b"\x00\x00" + b"data"
        header = b"ID3\x04\x00\x80" + synchsafe_bytes(len(frame))
        self.assertEqual([], tag_skim.picture_ranges(io.BytesIO(header + frame)))

    def test_files_without_pictures_have_no_holes(self):
        self.assertEqual([], tag_skim.picture_ranges(io.BytesIO(b"RIFF" + bytes(64))))


class SkimmingReaderTest(unittest.TestCase):
    def test_tags_and_durations_match_a_full_read_for_every_format(self):
        from mutagen import File
        for path in sorted(TAGS.iterdir()):
            with self.subTest(name=path.name), tag_skim.open_skimming(path) as handle:
                self.assertEqual(track_of(File(path)), track_of(File(handle)))

    def test_skipped_bytes_read_as_zeros_and_the_rest_as_the_file(self):
        path = TAGS / "id3v24.mp3"
        whole = path.read_bytes()
        with tag_skim.open_skimming(path) as handle:
            data = handle.read()
        self.assertEqual(bytes(874 - 226), data[226:874])
        self.assertEqual(whole[:226], data[:226])
        self.assertEqual(whole[874:], data[874:])

    def test_a_picture_is_not_read_from_the_disk(self):
        from mutagen import File
        path = TAGS / "tagged.flac"
        with tag_skim.open_skimming(path) as handle:
            File(handle)
            self.assertLess(handle.disk_bytes, path.stat().st_size - (926 - 292))


if __name__ == "__main__":
    unittest.main()

import io
import struct
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIRECTORY))

import cover_export
import library_index
from library_fixture import FIXTURE_TRACKS
from PIL import Image
from test_library_index import write_silent_flac

HEADER = struct.Struct("<4sHHHHI")
ENTRY = struct.Struct("<II")


def image_bytes(color, size=(40, 40), image_format="PNG"):
    buffer = io.BytesIO()
    Image.new("RGB", size, color).save(buffer, format=image_format)
    return buffer.getvalue()


def add_picture(path, data, picture_type):
    from mutagen.flac import FLAC, Picture
    audio = FLAC(path)
    picture = Picture()
    picture.type = picture_type
    picture.mime = "image/png"
    picture.data = data
    audio.add_picture(picture)
    audio.save()


class AlbumKeyTest(unittest.TestCase):
    def test_keys_hash_the_album_artist_a_separator_and_the_title(self):
        expected = cover_export.fnv1a(b"\x1fB", cover_export.fnv1a(b"A"))
        self.assertEqual(expected, cover_export.album_key("A", "B"))

    def test_keys_use_at_most_255_bytes_of_each_name(self):
        long_name = "x" * 300
        self.assertEqual(cover_export.album_key("x" * 255, "t"),
                         cover_export.album_key(long_name, "t"))

    def test_the_empty_hash_is_the_fnv_offset_basis(self):
        self.assertEqual(2166136261, cover_export.fnv1a(b""))


class PixelTest(unittest.TestCase):
    def test_colors_are_packed_as_rgb565(self):
        self.assertEqual(0xF800, cover_export.rgb565(255, 0, 0))
        self.assertEqual(0x07E0, cover_export.rgb565(0, 255, 0))
        self.assertEqual(0x001F, cover_export.rgb565(0, 0, 255))
        self.assertEqual(0xFFFF, cover_export.rgb565(255, 255, 255))

    def test_covers_are_cropped_to_a_centred_square(self):
        image = Image.new("RGB", (30, 10), (0, 0, 255))
        image.paste((255, 0, 0), (10, 0, 20, 10))
        pixels = cover_export.cover_pixels(image, 4)
        self.assertEqual(16, len(pixels))
        self.assertTrue(all(pixel == 0xF800 for pixel in pixels))


class ArtSearchTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.album = self.root / "Artist" / "Album"
        self.album.mkdir(parents=True)
        self.track = self.album / "01 - Song.flac"
        write_silent_flac(self.track, 1)

    def tearDown(self):
        self.directory.cleanup()

    def found_color(self):
        data = cover_export.find_art(self.root, ["Artist/Album/01 - Song.flac"])
        return None if data is None else Image.open(io.BytesIO(data)).convert("RGB").getpixel((0, 0))

    def test_an_embedded_front_cover_wins(self):
        add_picture(self.track, image_bytes((0, 255, 0)), 0)
        add_picture(self.track, image_bytes((255, 0, 0)), 3)
        self.assertEqual((255, 0, 0), self.found_color())

    def test_any_embedded_picture_is_used_without_a_front_cover(self):
        add_picture(self.track, image_bytes((0, 255, 0)), 0)
        self.assertEqual((0, 255, 0), self.found_color())

    def test_a_folder_image_is_the_fallback(self):
        (self.album / "Folder.JPG").write_bytes(image_bytes((0, 0, 255), image_format="JPEG"))
        red, green, blue = self.found_color()
        self.assertLess(red, 16)
        self.assertGreater(blue, 240)

    def test_an_album_without_art_has_none(self):
        self.assertIsNone(self.found_color())


class EncodeCoversTest(unittest.TestCase):
    def test_the_header_and_index_describe_sorted_records(self):
        red = Image.new("RGB", (8, 8), (255, 0, 0))
        blue = Image.new("RGB", (8, 8), (0, 0, 255))
        data = cover_export.encode_covers({900: red, 100: blue})
        magic, version, count, full, half, reserved = HEADER.unpack_from(data, 0)
        self.assertEqual((b"EMCV", 1, 2, 112, 56, 0), (magic, version, count, full, half, reserved))
        first_key, first_offset = ENTRY.unpack_from(data, HEADER.size)
        second_key, second_offset = ENTRY.unpack_from(data, HEADER.size + ENTRY.size)
        self.assertEqual((100, 900), (first_key, second_key))
        record = 112 * 112 * 2 + 56 * 56 * 2
        self.assertEqual(record, second_offset - first_offset)
        self.assertEqual(len(data), second_offset + record)
        self.assertEqual(0x001F, struct.unpack_from("<H", data, first_offset)[0])
        half_start = second_offset + 112 * 112 * 2
        self.assertEqual(0xF800, struct.unpack_from("<H", data, half_start)[0])


class AlbumCoversTest(unittest.TestCase):
    def test_every_album_gets_its_key_from_the_library_names(self):
        model = library_index.build_library(FIXTURE_TRACKS)
        albums = cover_export.album_sources(model)
        self.assertEqual(len(model.albums), len(albums))
        first = model.albums[0]
        artist = model.album_artists[first.album_artist].name
        self.assertEqual(cover_export.album_key(artist, first.title), albums[0].key)
        self.assertIn("AC DC/Back In Black/1-01 - Hells Bells.flac", albums[0].paths)


if __name__ == "__main__":
    unittest.main()

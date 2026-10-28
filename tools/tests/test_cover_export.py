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

    def test_only_the_first_track_is_read(self):
        second = self.album / "02 - Other.flac"
        write_silent_flac(second, 1)
        add_picture(second, image_bytes((255, 0, 0)), 3)
        data = cover_export.find_art(self.root, ["Artist/Album/01 - Song.flac",
                                                 "Artist/Album/02 - Other.flac"])
        self.assertIsNone(data)

    def test_folder_images_are_ordered_by_their_name_bytes_as_on_the_player(self):
        (self.album / "folder.jpg").write_bytes(image_bytes((0, 0, 255), image_format="JPEG"))
        (self.album / "Folder.png").write_bytes(image_bytes((255, 0, 0)))
        if len(list(self.album.glob("*older*"))) < 2:
            self.skipTest("the file system folds case")
        self.assertEqual("Folder.png", cover_export.folder_image(self.album).name)

    def test_a_missing_track_counts_as_no_art(self):
        (self.album / "Folder.JPG").write_bytes(image_bytes((0, 0, 255), image_format="JPEG"))
        data = cover_export.find_art(self.root, ["Artist/Album/missing.flac"])
        self.assertIsNotNone(data)


class FormatPictureTest(unittest.TestCase):
    TAGS = TOOLS_DIRECTORY.parent / "tests" / "fixtures" / "tags"

    def test_art_inside_mp4_ape_and_wma_files_is_found_as_a_front_cover(self):
        from tag_fixture import cover_jpeg
        for name in ("aac.m4a", "alac.m4a", "tagged.ape", "tagged.wma"):
            with self.subTest(name=name):
                pictures = cover_export.embedded_pictures(self.TAGS / name)
                self.assertEqual([(cover_export.FRONT_COVER, cover_jpeg())], pictures)

    def test_ogg_pictures_are_left_out_as_on_the_player(self):
        self.assertEqual([], cover_export.embedded_pictures(self.TAGS / "tagged.ogg"))

    def test_an_ape_back_cover_is_not_a_front_cover(self):
        self.assertEqual((cover_export.OTHER_PICTURE, b"jpeg"),
                         cover_export.ape_picture("Cover Art (Back)", b"back.jpg\x00jpeg"))
        self.assertIsNone(cover_export.ape_picture("Cover Art (Front)", b"no terminator"))

    def test_a_wma_picture_skips_its_mime_type_and_description(self):
        mime = "image/png".encode("utf-16-le") + b"\x00\x00"
        description = "x".encode("utf-16-le") + b"\x00\x00"
        data = struct.pack("<BI", 4, 3) + mime + description + b"png"
        self.assertEqual((4, b"png"), cover_export.asf_picture(data))
        self.assertIsNone(cover_export.asf_picture(struct.pack("<BI", 3, 99) + mime + b"\x00\x00"))


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


class ArtStampTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.album = self.root / "Artist" / "Album"
        self.album.mkdir(parents=True)
        self.track = self.album / "01 - Song.flac"
        write_silent_flac(self.track, 1)

    def tearDown(self):
        self.directory.cleanup()

    def expected(self, path, numbers):
        return cover_export.fnv1a(struct.pack("<III", *numbers), cover_export.fnv1a(path))

    def test_embedded_art_hashes_the_track_path_size_and_picture_length(self):
        picture = image_bytes((255, 0, 0))
        add_picture(self.track, picture, 3)
        art = cover_export.locate_art(self.root, ["Artist/Album/01 - Song.flac"])
        numbers = (self.track.stat().st_size, len(picture), cover_export.EMBEDDED_ART)
        self.assertEqual(self.expected(b"/Artist/Album/01 - Song.flac", numbers),
                         cover_export.stamp_for(art))

    def test_a_cover_file_hashes_its_card_path_size_and_time(self):
        image = self.album / "cover.png"
        image.write_bytes(image_bytes((0, 0, 255)))
        art = cover_export.locate_art(self.root, ["Artist/Album/01 - Song.flac"])
        stamp = cover_export.stamp_for(art, lambda path: (path.name.encode("utf-8"), 77))
        numbers = (image.stat().st_size, 77, cover_export.FILE_ART)
        self.assertEqual(self.expected(b"/Artist/Album/cover.png", numbers), stamp)

    def test_a_cover_file_at_the_top_of_the_card_has_one_slash(self):
        image = self.root / "cover.png"
        image.write_bytes(image_bytes((0, 0, 255)))
        write_silent_flac(self.root / "Song.flac", 1)
        art = cover_export.locate_art(self.root, ["Song.flac"])
        stamp = cover_export.stamp_for(art, lambda path: (path.name.encode("utf-8"), 5))
        numbers = (image.stat().st_size, 5, cover_export.FILE_ART)
        self.assertEqual(self.expected(b"/cover.png", numbers), stamp)

    def test_a_cover_file_without_the_players_time_gets_no_stamp(self):
        (self.album / "cover.png").write_bytes(image_bytes((0, 0, 255)))
        art = cover_export.locate_art(self.root, ["Artist/Album/01 - Song.flac"])
        self.assertEqual(cover_export.UNKNOWN_STAMP, cover_export.stamp_for(art))
        self.assertEqual(cover_export.UNKNOWN_STAMP, cover_export.stamp_for(art, lambda path: None))


class EncodeStampedTest(unittest.TestCase):
    def test_records_sit_in_the_players_slots_with_a_stamp_each(self):
        record = (bytes(range(256)) * 123)[:cover_export.RECORD_BYTES]
        capacity = cover_export.HEADER.size + 5 * cover_export.SLOT_BYTES + 100
        data = cover_export.encode_stamped({900: record, 100: record}, {900: 9, 100: 1}, capacity)
        slots, stamps_start, records_start = cover_export.stamped_layout(capacity)
        self.assertEqual(5, slots)
        self.assertEqual((b"EMCV", 1, 2, 112, 56, stamps_start), HEADER.unpack_from(data, 0))
        self.assertEqual((100, records_start), ENTRY.unpack_from(data, HEADER.size))
        self.assertEqual((900, records_start + cover_export.RECORD_BYTES),
                         ENTRY.unpack_from(data, HEADER.size + ENTRY.size))
        self.assertEqual((1, 9), struct.unpack_from("<II", data, stamps_start))
        self.assertEqual(records_start + 2 * cover_export.RECORD_BYTES, len(data))
        self.assertEqual(record, data[records_start:records_start + cover_export.RECORD_BYTES])

    def test_albums_past_the_last_slot_are_left_out(self):
        record = bytes(cover_export.RECORD_BYTES)
        capacity = cover_export.stamped_size(1)
        data = cover_export.encode_stamped({1: record, 2: record}, {}, capacity)
        self.assertEqual(1, HEADER.unpack_from(data, 0)[2])
        self.assertEqual(capacity, len(data))
        self.assertEqual(0, struct.unpack_from("<I", data, cover_export.stamped_layout(capacity)[1])[0])

    def test_a_record_of_the_wrong_size_is_refused(self):
        with self.assertRaises(ValueError):
            cover_export.encode_stamped({1: b"short"}, {}, cover_export.stamped_size(1))


class AlbumCoversTest(unittest.TestCase):
    def test_every_album_gets_its_key_from_the_library_names(self):
        model = library_index.build_library(FIXTURE_TRACKS)
        albums = cover_export.album_sources(model)
        self.assertEqual(len(model.albums), len(albums))
        first = model.albums[0]
        artist = model.album_artists[first.album_artist].name
        self.assertEqual(cover_export.album_key(artist, first.title), albums[0].key)
        self.assertIn("AC DC/Back In Black/1-01 - Hells Bells.flac", albums[0].paths)


class LibraryFileSourcesTest(unittest.TestCase):
    def test_sources_read_from_a_library_file_match_the_scanned_ones(self):
        model = library_index.build_library(FIXTURE_TRACKS)
        data = library_index.encode_library(model)
        self.assertEqual(cover_export.album_sources(model),
                         cover_export.album_sources_from_library(data))

    def test_a_file_that_is_not_a_library_is_refused(self):
        with self.assertRaises(ValueError):
            cover_export.album_sources_from_library(b"NOPE" + bytes(200))

    def test_the_command_uses_the_library_file_instead_of_scanning(self):
        model = library_index.build_library(FIXTURE_TRACKS)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            library = root / "library.db"
            library.write_bytes(library_index.encode_library(model))
            output = root / "covers.bin"
            original = library_index.scan
            library_index.scan = lambda *_: self.fail("scanned the music again")
            try:
                status = cover_export.main(["--music", str(root), "--library", str(library),
                                            "--output", str(output)])
            finally:
                library_index.scan = original
            self.assertEqual(0, status)
            self.assertEqual(b"EMCV", output.read_bytes()[:4])


if __name__ == "__main__":
    unittest.main()

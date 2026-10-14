import struct
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIRECTORY))

import library_index
from library_fixture import FIXTURE_TRACKS as SAMPLE_TRACKS

HEADER_SIZE = 128
SECTION_ENTRY = struct.Struct("<IIHH")
TRACK_RECORD = struct.Struct("<IIHHHHIBBH")
ALBUM_RECORD = struct.Struct("<IHHHHHH")
GROUP_RECORD = struct.Struct("<IIHH")
FOLDER_RECORD = struct.Struct("<IHHIHH")


def write_silent_flac(path, seconds):
    sample_rate, channels, bits = 44100, 2, 16
    packed = (sample_rate << 44) | ((channels - 1) << 41) | ((bits - 1) << 36)
    packed |= sample_rate * seconds
    info = struct.pack(">HH", 4096, 4096) + bytes(6) + packed.to_bytes(8, "big") + bytes(16)
    header = bytes([0x80]) + len(info).to_bytes(3, "big")
    path.write_bytes(b"fLaC" + header + info)


class SortKeyTest(unittest.TestCase):
    def test_case_is_ignored(self):
        self.assertEqual(library_index.sort_key("abba"), library_index.sort_key("ABBA"))

    def test_a_leading_the_is_ignored(self):
        self.assertEqual("beatles", library_index.sort_key("The Beatles"))
        self.assertEqual("theatre", library_index.sort_key("Theatre"))
        self.assertEqual("the", library_index.sort_key("The"))

    def test_accents_are_ignored(self):
        self.assertEqual("bjork", library_index.sort_key("Björk"))


class TagParsingTest(unittest.TestCase):
    def test_numbers_take_the_leading_integer(self):
        self.assertEqual(1, library_index.parse_number("1/10"))
        self.assertEqual(7, library_index.parse_number("07"))
        self.assertEqual(0, library_index.parse_number(""))
        self.assertEqual(0, library_index.parse_number("x"))

    def test_years_take_the_first_four_digits(self):
        self.assertEqual(1980, library_index.parse_year("1980-08-01"))
        self.assertEqual(2025, library_index.parse_year("2025"))
        self.assertEqual(0, library_index.parse_year(""))
        self.assertEqual(0, library_index.parse_year("19"))

    def test_file_names_can_carry_disc_and_track_numbers(self):
        self.assertEqual((1, 7), library_index.numbers_from_filename("1-07 - Get It Hot"))
        self.assertEqual((0, 7), library_index.numbers_from_filename("07 - Shoots"))
        self.assertEqual((0, 0), library_index.numbers_from_filename("Intro"))

    def test_titles_from_file_names_drop_the_number_prefix(self):
        self.assertEqual("Get It Hot", library_index.title_from_filename("1-07 - Get It Hot"))
        self.assertEqual("Shoots", library_index.title_from_filename("07 - Shoots"))
        self.assertEqual("Intro", library_index.title_from_filename("Intro"))


class TrackFromTagsTest(unittest.TestCase):
    def test_tags_are_used_when_present(self):
        tags = {"title": ["Hells Bells"], "artist": ["AC/DC"], "albumartist": ["AC/DC"],
                "album": ["Back In Black"], "genre": ["Metal"], "date": ["1980-08-01"],
                "discnumber": ["1"], "tracknumber": ["1/10"]}
        track = library_index.track_from_tags("AC DC/Back In Black/x.flac", tags, 312.8)
        self.assertEqual(("Hells Bells", "AC/DC", "AC/DC"),
                         (track.title, track.artist, track.album_artist))
        self.assertEqual(("Back In Black", "Metal", 1980), (track.album, track.genre, track.year))
        self.assertEqual((1, 1, 312800), (track.disc, track.number, track.duration_ms))

    def test_missing_tags_fall_back_to_the_path(self):
        track = library_index.track_from_tags("Artist/Some Album/1-03 - A Song.flac", {}, 1.0)
        self.assertEqual("A Song", track.title)
        self.assertEqual("Some Album", track.album)
        self.assertEqual("Unknown Artist", track.artist)
        self.assertEqual("Unknown Artist", track.album_artist)
        self.assertEqual("Unknown Genre", track.genre)
        self.assertEqual((1, 3), (track.disc, track.number))

    def test_artists_fill_in_for_each_other(self):
        only_album_artist = library_index.track_from_tags("a/b/c.flac", {"albumartist": ["X"]}, 1)
        only_artist = library_index.track_from_tags("a/b/c.flac", {"artist": ["Y"]}, 1)
        self.assertEqual("X", only_album_artist.artist)
        self.assertEqual("Y", only_artist.album_artist)


class BuildLibraryTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.library = library_index.build_library(SAMPLE_TRACKS)

    def album_titles(self, indices):
        return [self.library.albums[index].title for index in indices]

    def test_albums_are_sorted_by_title(self):
        titles = [album.title for album in self.library.albums]
        self.assertEqual(["Back In Black", "The Best of Something", "High Voltage", "Homogenic",
                          "This Won't Be The Last Time"], titles)

    def test_album_tracks_are_contiguous_and_in_track_order(self):
        album = self.library.albums[0]
        tracks = self.library.tracks[album.first_track:album.first_track + album.track_count]
        self.assertEqual(["Hells Bells", "Shoot to Thrill"], [track.title for track in tracks])

    def test_artists_are_sorted_and_list_albums_chronologically(self):
        names = [artist.name for artist in self.library.artists]
        self.assertEqual(["AC/DC", "acloudyskye", "The Beatles", "Björk"], names)
        bjork = self.library.artists[3]
        self.assertEqual(["Homogenic", "The Best of Something"], self.album_titles(bjork.albums))
        self.assertEqual(["High Voltage", "Back In Black"],
                         self.album_titles(self.library.artists[0].albums))

    def test_album_artists_group_compilations(self):
        names = [group.name for group in self.library.album_artists]
        self.assertEqual(["AC/DC", "acloudyskye", "Björk", "Various Artists"], names)

    def test_genres_include_unknown_genre(self):
        names = [genre.name for genre in self.library.genres]
        self.assertEqual(["Electronic", "Metal", "Pop", "Unknown Genre"], names)
        self.assertEqual(["Back In Black", "High Voltage"],
                         self.album_titles(self.library.genres[1].albums))

    def test_title_order_sorts_all_songs(self):
        titles = [self.library.tracks[index].title for index in self.library.title_order]
        self.assertEqual(["Army of Me", "Hells Bells", "Help!", "Hunter",
                          "It's a Long Way to the Top", "Jóga", "Shoot to Thrill", "Shoots"],
                         titles)

    def test_folders_form_a_sorted_tree(self):
        root = self.library.folders[0]
        self.assertEqual("", root.name)
        names = [self.library.folders[index].name for index in root.subfolders]
        self.assertEqual(["AC DC", "acloudyskye", "Björk", "Various Artists"], names)
        homogenic = next(folder for folder in self.library.folders if folder.name == "Homogenic")
        self.assertEqual(["Hunter", "Jóga"],
                         [self.library.tracks[index].title for index in homogenic.tracks])


class EncodeLibraryTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.library = library_index.build_library(SAMPLE_TRACKS)
        cls.data = library_index.encode_library(cls.library)

    def section(self, number):
        return SECTION_ENTRY.unpack_from(self.data, 8 + number * SECTION_ENTRY.size)

    def string_at(self, offset):
        strings_offset = self.section(8)[0]
        start = strings_offset + offset
        return self.data[start:self.data.index(b"\0", start)].decode("utf-8")

    def test_the_header_identifies_the_format(self):
        magic, version, sections = struct.unpack_from("<4sHH", self.data, 0)
        self.assertEqual((b"EMDB", 1, 9), (magic, version, sections))
        self.assertEqual(HEADER_SIZE, self.section(0)[0])

    def test_sections_declare_their_counts_and_record_sizes(self):
        expected = [(8, 24), (5, 16), (4, 12), (4, 12), (4, 12)]
        for number, (count, size) in enumerate(expected):
            _, actual_count, actual_size, _ = self.section(number)
            self.assertEqual((count, size), (actual_count, actual_size))

    def test_track_records_round_trip(self):
        offset = self.section(0)[0]
        fields = TRACK_RECORD.unpack_from(self.data, offset)
        self.assertEqual("Hells Bells", self.string_at(fields[0]))
        self.assertEqual("AC DC/Back In Black/1-01 - Hells Bells.flac", self.string_at(fields[1]))
        self.assertEqual((0, 0, 1), (fields[2], fields[3], fields[4]))
        self.assertEqual((60000, 1, 1), (fields[6], fields[7], fields[8]))

    def test_album_records_round_trip(self):
        fields = ALBUM_RECORD.unpack_from(self.data, self.section(1)[0] + ALBUM_RECORD.size)
        self.assertEqual("The Best of Something", self.string_at(fields[0]))
        self.assertEqual((3, 2, 2001), (fields[1], fields[2], fields[3]))
        self.assertEqual(0xFFFF, fields[6])

    def test_group_links_point_at_albums(self):
        links_offset = self.section(7)[0]
        name, first_link, count, _ = GROUP_RECORD.unpack_from(
            self.data, self.section(2)[0] + 3 * GROUP_RECORD.size)
        self.assertEqual("Björk", self.string_at(name))
        albums = struct.unpack_from(f"<{count}H", self.data, links_offset + 2 * first_link)
        self.assertEqual((3, 1), albums)

    def test_strings_are_stored_once(self):
        strings_offset, strings_size, _, _ = self.section(8)
        blob = self.data[strings_offset:strings_offset + strings_size]
        self.assertEqual(0, blob[0])
        self.assertEqual(1, blob.count(b"\0AC/DC\0"))


class ReadTrackTest(unittest.TestCase):
    def test_tags_and_duration_are_read_from_a_flac_file(self):
        from mutagen.flac import FLAC
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "Artist" / "Album" / "01 - Song.flac"
            path.parent.mkdir(parents=True)
            write_silent_flac(path, 3)
            audio = FLAC(path)
            audio["title"] = "Tagged Title"
            audio["artist"] = "Tagged Artist"
            audio.save()
            track = library_index.read_track(root, path)
        self.assertEqual("Artist/Album/01 - Song.flac", track.path)
        self.assertEqual(("Tagged Title", "Tagged Artist"), (track.title, track.artist))
        self.assertEqual(3000, track.duration_ms)


if __name__ == "__main__":
    unittest.main()

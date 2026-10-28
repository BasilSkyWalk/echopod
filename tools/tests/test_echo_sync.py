import contextlib
import io
import os
import struct
import sys
import tempfile
import unicodedata
import unittest
from datetime import datetime, timezone
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIRECTORY))

import cover_export
import echo_sync
import library_index
from test_cover_export import image_bytes
from test_library_index import write_silent_flac

SCAN_RECORD = struct.Struct("<4sIII16s")
MEBIBYTE = 1024 * 1024
FIXED_UTC = echo_sync.CardVolume("FAT32", True, False, 0, "")
EXFAT = echo_sync.CardVolume("exFAT", True, True, 0, "")


class MemoryCard:
    def __init__(self):
        self.nodes = []

    def add(self, parent, name, directory, size, modified):
        self.nodes.append([parent, name, directory, size, modified])

    def node(self, name):
        return next(node for node in self.nodes if node[1] == name)

    def list_folder(self, path):
        if path != "/" and not any(node[0] == path for node in self.nodes):
            return None
        return [echo_sync.CardEntry(name.encode("utf-8"), directory, size, modified,
                                    echo_sync.child_path(parent.encode(), name.encode()).decode())
                for parent, name, directory, size, modified in self.nodes if parent == path]

    def fingerprint(self):
        return echo_sync.music_fingerprint(self.list_folder, "/")


def players_test_card():
    card = MemoryCard()
    card.add("/", "AC DC", True, 0, 1)
    card.add("/AC DC", "Back In Black", True, 0, 2)
    card.add("/AC DC/Back In Black", "01 - Hells Bells.flac", False, 31000000, 100)
    card.add("/AC DC/Back In Black", "02 - Shoot to Thrill.flac", False, 29000000, 101)
    card.add("/AC DC/Back In Black", "cover.jpg", False, 80000, 102)
    card.add("/", "Mix.m3u8", False, 400, 3)
    card.add("/", ".echo", True, 0, 4)
    card.add("/.echo", "library.db", False, 4194304, 5)
    return card


class PlayerFingerprintTest(unittest.TestCase):
    def test_the_players_test_card_gives_the_fingerprint_the_player_stores(self):
        self.assertEqual(echo_sync.Fingerprint(True, 3518657026, 2077149385, 6),
                         players_test_card().fingerprint())

    def test_folders_that_cannot_be_opened_count_as_empty_as_on_the_player(self):
        card = players_test_card()
        card.add("/", "Empty", True, 0, 6)
        card.add("/AC DC", "Live", True, 0, 7)
        self.assertEqual(echo_sync.Fingerprint(True, 732559337, 1706284886, 8),
                         card.fingerprint())

    def test_hidden_entries_do_not_count(self):
        card = players_test_card()
        before = card.fingerprint()
        card.add("/", ".fseventsd", True, 0, 9)
        card.add("/.fseventsd", "0000000001", False, 77, 9)
        card.add("/AC DC/Back In Black", "._01 - Hells Bells.flac", False, 4096, 9)
        card.node("library.db")[4] = 999
        self.assertEqual(before, card.fingerprint())

    def test_files_the_library_ignores_do_not_count(self):
        card = players_test_card()
        before = card.fingerprint()
        card.add("/", "probe1.bin", False, 65536, 9)
        card.add("/AC DC/Back In Black", "notes.txt", False, 120, 9)
        card.add("/AC DC/Back In Black", "scan 01.jpg", False, 900000, 9)
        card.add("/", "System Volume Information", True, 0, 9)
        card.add("/System Volume Information", "WPSettings.dat", False, 12, 9)
        self.assertEqual(before, card.fingerprint())

    def test_the_counted_names_are_the_players(self):
        counted = ["a.flac", "a.ogg", "a.mp3", "a.m4a", "a.wav", "a.wma", "a.ape", "a.dsf",
                   "a.dff", "A.FLAC", "b.m3u", "b.M3U8", "Folder.JPG", "front.png",
                   "albumart.jpeg", "cover.png"]
        ignored = ["x.aac", "x.txt", "x.opus", "x.aiff", "x.mp4", "x.flac.txt", "scan 01.jpg",
                   "covers.jpg", "folder.gif"]
        for name in counted:
            self.assertTrue(echo_sync.fingerprint_counts(name.encode(), False), name)
        for name in ignored:
            self.assertFalse(echo_sync.fingerprint_counts(name.encode(), False), name)
        self.assertTrue(echo_sync.fingerprint_counts(b"notes.txt", True))
        self.assertFalse(echo_sync.fingerprint_counts(b"SYSTEM VOLUME INFORMATION", True))

    def test_a_changed_stamp_size_or_name_changes_it(self):
        before = players_test_card().fingerprint()
        for index, field in ((2, 4), (4, 3)):
            card = players_test_card()
            card.nodes[index][field] += 1
            self.assertNotEqual(before, card.fingerprint())
        card = players_test_card()
        card.node("Mix.m3u8")[1] = "Mix 2.m3u8"
        self.assertNotEqual(before, card.fingerprint())

    def test_a_card_deeper_than_the_player_walks_is_incomplete(self):
        card = MemoryCard()
        parent = "/"
        for _ in range(echo_sync.MAX_DEPTH + 1):
            card.add(parent, "d", True, 0, 1)
            parent = echo_sync.child_path(parent.encode(), b"d").decode()
        self.assertFalse(card.fingerprint().complete)

    def test_the_deepest_folder_the_player_walks_is_complete(self):
        card = MemoryCard()
        parent = "/"
        for _ in range(echo_sync.MAX_DEPTH - 1):
            card.add(parent, "d", True, 0, 1)
            parent = echo_sync.child_path(parent.encode(), b"d").decode()
        self.assertTrue(card.fingerprint().complete)

    def test_a_path_longer_than_the_player_holds_is_incomplete(self):
        card = MemoryCard()
        card.add("/", "a" * 1022, True, 0, 1)
        self.assertTrue(card.fingerprint().complete)
        card = MemoryCard()
        card.add("/", "a" * 1023, True, 0, 1)
        self.assertFalse(card.fingerprint().complete)

    def test_a_card_without_a_root_is_incomplete(self):
        self.assertFalse(echo_sync.music_fingerprint(lambda location: None, "/").complete)


class StampTest(unittest.TestCase):
    def test_dates_and_times_pack_like_a_fat_directory_entry(self):
        seconds = int(datetime(2026, 1, 1, tzinfo=timezone.utc).timestamp())
        self.assertEqual(0x5C210000, echo_sync.dos_stamp(seconds))
        seconds = int(datetime(2024, 7, 9, 13, 45, 59, tzinfo=timezone.utc).timestamp())
        expected = ((2024 - 1980) << 9 | 7 << 5 | 9) << 16 | 13 << 11 | 45 << 5 | 29
        self.assertEqual(expected, echo_sync.dos_stamp(seconds))

    def test_times_fat_cannot_hold_are_zero(self):
        self.assertEqual(0, echo_sync.dos_stamp(0))
        self.assertEqual(0, echo_sync.dos_stamp(-11644473600))

    def test_a_fixed_offset_shifts_utc_to_the_cards_clock(self):
        volume = echo_sync.CardVolume("FAT32", True, False, 3600, "")
        stat = FakeStat(datetime(2025, 3, 1, 10, 0, 0, tzinfo=timezone.utc))
        entry = echo_sync.host_entry(FakeEntry("a.flac", stat), volume)
        self.assertEqual(echo_sync.dos_stamp(stat.st_mtime_ns // 10 ** 9 + 3600), entry.modified)

    def test_hundredths_of_a_second_are_dropped_as_the_player_does(self):
        written = datetime(2025, 3, 1, 10, 0, 0, tzinfo=timezone.utc)
        exact = echo_sync.host_entry(FakeEntry("a.flac", FakeStat(written)), FIXED_UTC)
        later = FakeStat(written, extra_ns=1_990_000_000)
        self.assertEqual(exact.modified,
                         echo_sync.host_entry(FakeEntry("a.flac", later), FIXED_UTC).modified)

    def test_exfat_times_are_utc_half_seconds_as_the_player_computes(self):
        written = datetime(2026, 7, 11, 13, 32, 22, tzinfo=timezone.utc)
        entry = echo_sync.host_entry(FakeEntry("a.flac", FakeStat(written)), EXFAT)
        self.assertEqual(891888371, entry.modified)
        later = FakeStat(written, extra_ns=1_990_000_000)
        self.assertEqual(891888371, echo_sync.host_entry(FakeEntry("a.flac", later), EXFAT).modified)

    def test_exfat_needs_no_time_zone_guess_on_any_computer(self):
        for platform in ("win32", "darwin", "linux"):
            volume = echo_sync.card_volume("exFAT", "", platform)
            self.assertTrue(volume.exfat)
            self.assertEqual("", volume.caveat)

    def test_exfat_files_of_4_gib_or_more_are_left_out_as_the_player_does(self):
        volume = EXFAT
        stat = FakeStat(datetime(2025, 1, 1, tzinfo=timezone.utc), size=1 << 32)
        self.assertIsNone(echo_sync.host_entry(FakeEntry("big.wav", stat), volume))
        self.assertIsNotNone(echo_sync.host_entry(FakeEntry("big.wav", stat), FIXED_UTC))

    def test_folders_count_with_size_zero(self):
        stat = FakeStat(datetime(2025, 1, 1, tzinfo=timezone.utc), size=4096)
        entry = echo_sync.host_entry(FakeEntry("Album", stat, directory=True), FIXED_UTC)
        self.assertEqual((True, 0), (entry.directory, entry.size))


class FakeStat:
    def __init__(self, moment, size=1000, birth=None, extra_ns=0):
        self.st_mtime_ns = int(moment.timestamp()) * 10 ** 9 + extra_ns
        self.st_mtime = self.st_mtime_ns / 10 ** 9
        self.st_birthtime = self.st_mtime if birth is None else birth
        self.st_size = size


class FakeEntry:
    def __init__(self, name, stat, directory=False):
        self.name, self.path, self.stat_result, self.directory = name, name, stat, directory

    def stat(self, follow_symlinks=True):
        return self.stat_result

    def is_dir(self, follow_symlinks=True):
        return self.directory


class NameTest(unittest.TestCase):
    def test_names_are_utf8(self):
        self.assertEqual("Ältere.m3u".encode("utf-8"), echo_sync.card_name("Ältere.m3u", "win32"))

    def test_a_mac_lists_names_decomposed_and_they_are_recomposed(self):
        decomposed = unicodedata.normalize("NFD", "Ältere.m3u")
        self.assertEqual("Ältere.m3u".encode("utf-8"), echo_sync.card_name(decomposed, "darwin"))

    def test_an_unpaired_surrogate_is_encoded_as_the_player_does(self):
        self.assertEqual(b"a\xed\xa0\x80", echo_sync.card_name("a\ud800", "win32"))


class HostCardTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)

    def tearDown(self):
        self.directory.cleanup()

    def make(self, relative, data=b"x", moment=datetime(2024, 5, 6, 7, 8, 10, tzinfo=timezone.utc)):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        seconds = moment.timestamp()
        os.utime(path, (seconds, seconds))
        return path

    def listed_card(self):
        card = MemoryCard()
        folders = [(str(self.root), "/")]
        while folders:
            folder, card_folder = folders.pop(0)
            for entry in os.scandir(folder):
                stat = entry.stat()
                modified = echo_sync.dos_stamp(stat.st_mtime_ns // 10 ** 9)
                card.add(card_folder, entry.name, entry.is_dir(),
                         0 if entry.is_dir() else stat.st_size, modified)
                if entry.is_dir():
                    path = echo_sync.child_path(card_folder.encode(), entry.name.encode())
                    folders.append((entry.path, path.decode()))
        return card

    def test_a_folder_on_disk_gives_the_fingerprint_of_its_listing(self):
        self.make("Artist/Album/01 - Song.flac", b"12345")
        self.make("Artist/Album/cover.jpg", b"123")
        self.make("Artist/Album/notes.txt")
        self.make(".echo/library.db")
        self.make("Mix.m3u8", b"#EXTM3U")
        computed = echo_sync.card_fingerprint(self.root, FIXED_UTC)
        self.assertEqual(self.listed_card().fingerprint(), computed)
        self.assertEqual(5, computed.entries)


class CardFileTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.path = Path(self.directory.name) / "file.bin"
        self.card_file = echo_sync.CardFile("file.bin", 64 * 1024, 16)

    def tearDown(self):
        self.directory.cleanup()

    def test_the_standard_sizes_are_the_ones_the_player_reserves(self):
        self.assertEqual(4 * MEBIBYTE, echo_sync.LIBRARY_FILE.standard_size)
        self.assertEqual(80 * MEBIBYTE, echo_sync.COVERS_FILE.standard_size)
        self.assertEqual(4096, echo_sync.SCAN_FILE.standard_size)

    def test_a_missing_file_is_created_at_the_standard_size(self):
        data = bytes(range(40))
        self.assertEqual(64 * 1024, echo_sync.write_card_file(self.path, data, self.card_file))
        written = self.path.read_bytes()
        self.assertEqual(64 * 1024, len(written))
        self.assertEqual(data, written[:40])
        self.assertEqual(bytes(64 * 1024 - 40), written[40:])

    def test_an_existing_file_is_written_in_place_and_keeps_its_size(self):
        self.path.write_bytes(b"\xAA" * 100_000)
        data = bytes(range(1, 41))
        self.assertEqual(100_000, echo_sync.write_card_file(self.path, data, self.card_file))
        written = self.path.read_bytes()
        self.assertEqual(100_000, len(written))
        self.assertEqual(data, written[:40])
        self.assertEqual(b"\xAA" * (100_000 - 40), written[40:])

    def test_a_file_too_small_for_the_data_grows_to_the_standard_size(self):
        self.path.write_bytes(b"\xAA" * 10)
        echo_sync.write_card_file(self.path, bytes(range(1, 41)), self.card_file)
        self.assertEqual(64 * 1024, self.path.stat().st_size)

    def test_data_bigger_than_the_standard_size_gets_a_file_of_its_size(self):
        data = b"\x01" * (100 * 1024)
        echo_sync.write_card_file(self.path, data, self.card_file)
        self.assertEqual(data, self.path.read_bytes())

    def test_forgetting_the_fingerprint_clears_only_its_magic(self):
        record = SCAN_RECORD.pack(b"MESC", 1, 2, 3, b"v1.2.3".ljust(16, b"\0"))
        self.path.write_bytes(record + bytes(4096 - len(record)))
        echo_sync.forget_fingerprint(self.path)
        written = self.path.read_bytes()
        self.assertEqual(bytes(4) + record[4:], written[:len(record)])
        self.assertEqual(4096, len(written))


class ScanRecordTest(unittest.TestCase):
    def test_the_record_holds_the_magic_both_hashes_the_count_and_the_firmware_tag(self):
        tag = b"0.1.0".ljust(16, b"\0")
        record = echo_sync.scan_record(echo_sync.Fingerprint(True, 0x11223344, 5, 6), tag)
        self.assertEqual(b"MESC" + bytes([0x44, 0x33, 0x22, 0x11]) + struct.pack("<II", 5, 6) + tag,
                         record)

    def test_a_stored_record_reads_back(self):
        fingerprint = echo_sync.Fingerprint(True, 1, 2, 3)
        record = echo_sync.scan_record(fingerprint, bytes(16))
        self.assertEqual(fingerprint, echo_sync.stored_fingerprint(record))
        self.assertIsNone(echo_sync.stored_fingerprint(bytes(32)))

    def test_the_firmware_tag_is_kept_or_empty(self):
        record = SCAN_RECORD.pack(b"MESC", 1, 2, 3, b"tag".ljust(16, b"\0"))
        self.assertEqual(b"tag".ljust(16, b"\0"), echo_sync.stored_version(record))
        self.assertEqual(bytes(16), echo_sync.stored_version(b""))


class SyncTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.card = Path(self.directory.name)
        self.echo = self.card / ".echo"
        album = self.card / "Artist" / "Album"
        album.mkdir(parents=True)
        write_silent_flac(album / "01 - Song.flac", 1)
        (album / "cover.png").write_bytes(image_bytes((255, 0, 0)))

    def tearDown(self):
        self.directory.cleanup()

    def run_sync(self, *arguments):
        output = io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
            status = echo_sync.main([str(self.card), *arguments])
        return status, output.getvalue()

    def prepare_echo(self, covers_size=MEBIBYTE):
        self.echo.mkdir()
        (self.echo / "covers.bin").write_bytes(bytes(covers_size))

    def test_a_sync_writes_the_library_the_covers_and_the_fingerprint(self):
        self.prepare_echo()
        status, output = self.run_sync()
        self.assertEqual(0, status, output)
        library = (self.echo / "library.db").read_bytes()
        expected = library_index.encode_library(library_index.build_library(
            *library_index.scan(self.card)[:2]))
        self.assertEqual(4 * MEBIBYTE, len(library))
        self.assertEqual(expected, library[:len(expected)])
        covers = (self.echo / "covers.bin").read_bytes()
        self.assertEqual(MEBIBYTE, len(covers))
        self.assertEqual((b"EMCV", 1, 1), struct.unpack_from("<4sHH", covers))
        scan = (self.echo / "scan.bin").read_bytes()
        self.assertEqual(4096, len(scan))
        volume = echo_sync.detect_volume(self.card)
        self.assertEqual(echo_sync.card_fingerprint(self.card, volume),
                         echo_sync.stored_fingerprint(scan))
        self.assertIn("Eject the card safely", output.replace("eject", "Eject"))
        self.assertEqual(0, self.run_sync("--check")[0])

    def test_covers_use_the_players_layout_for_the_cards_file(self):
        self.prepare_echo()
        status, output = self.run_sync()
        self.assertEqual(0, status, output)
        covers = (self.echo / "covers.bin").read_bytes()
        slots, stamps_start, records_start = cover_export.stamped_layout(MEBIBYTE)
        self.assertEqual(stamps_start, struct.unpack_from("<I", covers, 12)[0])
        self.assertEqual(records_start, struct.unpack_from("<I", covers, 20)[0])
        art = cover_export.locate_art(self.card, ["Artist/Album/01 - Song.flac"])
        describe = echo_sync.art_describer(echo_sync.detect_volume(self.card))
        stamp = cover_export.stamp_for(art, describe)
        self.assertEqual(stamp, struct.unpack_from("<I", covers, stamps_start)[0])

    def test_cover_files_are_described_with_the_players_name_and_time(self):
        image = self.card / "Artist" / "Album" / "cover.png"
        describe = echo_sync.art_describer(FIXED_UTC)
        expected = echo_sync.card_stamp(image.stat(), FIXED_UTC)
        self.assertEqual((b"cover.png", expected), describe(image))
        unsupported = echo_sync.CardVolume("NTFS", False, False, 0, "")
        self.assertIsNone(echo_sync.art_describer(unsupported))

    def test_the_firmware_tag_in_scan_bin_is_kept(self):
        self.prepare_echo()
        tag = b"0.1.0-abc".ljust(16, b"\0")
        (self.echo / "scan.bin").write_bytes(SCAN_RECORD.pack(b"MESC", 1, 2, 3, tag) + bytes(4064))
        self.run_sync()
        scan = (self.echo / "scan.bin").read_bytes()
        self.assertEqual(tag, scan[16:32])
        self.assertEqual(4096, len(scan))

    def test_a_dry_run_writes_nothing(self):
        self.prepare_echo()
        status, output = self.run_sync("--dry-run")
        self.assertEqual(0, status, output)
        self.assertEqual(["covers.bin"], sorted(path.name for path in self.echo.iterdir()))
        self.assertEqual(bytes(MEBIBYTE), (self.echo / "covers.bin").read_bytes())

    def test_without_a_fingerprint_scan_bin_is_left_alone(self):
        self.prepare_echo()
        record = SCAN_RECORD.pack(b"MESC", 1, 2, 3, bytes(16))
        (self.echo / "scan.bin").write_bytes(record)
        self.run_sync("--no-fingerprint")
        self.assertEqual(record, (self.echo / "scan.bin").read_bytes())
        self.assertTrue((self.echo / "library.db").exists())

    def test_covers_only_uses_the_cards_library(self):
        self.prepare_echo()
        self.run_sync("--no-covers")
        self.assertEqual(bytes(MEBIBYTE), (self.echo / "covers.bin").read_bytes())
        original = library_index.scan
        library_index.scan = lambda *_: self.fail("scanned the music again")
        try:
            status, output = self.run_sync("--covers-only")
        finally:
            library_index.scan = original
        self.assertEqual(0, status, output)
        self.assertEqual(b"EMCV", (self.echo / "covers.bin").read_bytes()[:4])

    def test_a_folder_that_is_not_a_card_is_refused(self):
        status, output = self.run_sync()
        self.assertEqual(2, status)
        self.assertIn("isn't the top of a memory card", output)
        self.assertFalse(self.echo.exists())


class AskForCardTest(unittest.TestCase):
    def test_dragged_and_quoted_paths_are_cleaned_up(self):
        self.assertEqual("E:\\Music", echo_sync.typed_path('  "E:\\Music" ', "win32"))
        self.assertEqual("/Volumes/MY CARD", echo_sync.typed_path("/Volumes/MY\\ CARD ", "darwin"))
        self.assertEqual("/Volumes/MY CARD", echo_sync.typed_path("'/Volumes/MY CARD'", "darwin"))

    def test_only_folders_with_an_echo_folder_are_offered(self):
        with tempfile.TemporaryDirectory() as directory:
            player, other = Path(directory, "player"), Path(directory, "other")
            (player / ".echo").mkdir(parents=True)
            other.mkdir()
            self.assertEqual([player], echo_sync.player_cards([other, player,
                                                               Path(directory, "missing")]))

    def test_one_known_card_is_the_default(self):
        card = Path("/Volumes/CARD")
        self.assertEqual(str(card), echo_sync.ask_for_card([card], read=lambda prompt: ""))
        self.assertEqual("/elsewhere",
                         echo_sync.ask_for_card([card], read=lambda prompt: "/elsewhere"))

    def test_an_empty_answer_without_a_known_card_writes_nothing(self):
        with self.assertRaises(echo_sync.SyncError):
            echo_sync.ask_for_card([], read=lambda prompt: " ")

    def test_the_card_can_be_left_out_of_the_command(self):
        self.assertIsNone(echo_sync.parse_arguments([]).card)


if __name__ == "__main__":
    unittest.main()

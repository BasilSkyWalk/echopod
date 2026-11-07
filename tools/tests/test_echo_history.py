import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
SHARED_DIRECTORY = TOOLS_DIRECTORY.parent / "tests" / "fixtures" / "history"
sys.path.insert(0, str(TOOLS_DIRECTORY))

import echo_history
from echo_history import COMPUTER, PLAY, Record

NOW = 1_700_000_000
MINUTE_MS = 60_000


def play(sequence, session, start_ms, played_ms, duration_ms=240_000, title="Song", start_at=0,
         end_at=None, artist="Artist", album="Album", number=1, unix_time=0):
    end_at = start_at + played_ms if end_at is None else end_at
    return Record(sequence, session, PLAY, number, start_ms, start_ms + played_ms, unix_time,
                  played_ms, duration_ms, start_at, end_at, artist, album, title)


def mark(sequence, session, at_ms):
    return Record(sequence, session, COMPUTER, 0, at_ms, at_ms, 0, 0, 0, 0, 0)


def timed(record, boot_ms):
    record.unix_time = (boot_ms + record.end_ms) // 1000
    return record


SECOND_BOOT_MS = 1_790_000_000_000
THIRD_BOOT_MS = 1_790_100_000_000
EXISTING_LOG = "#AUDIOSCROBBLER/1.1\n#TZ/UTC\n#CLIENT/Other\nA\tB\tC\t\t100\tL\t5"


def shared_records():
    """The ring tests/app/test_scrobbler_log.c reads too (tests/fixtures/history)."""
    paused = dict(duration_ms=300_000, title="Paused Song", artist="\u00dcn\u00efcode",
                  album="\u00c1lbum", number=3)
    long_song = dict(duration_ms=600_000, title="Long Song", artist="Band", album="Epic",
                     number=7)
    return [play(1, 1, 10_000, 200_000, duration_ms=312_400, title="Hells Bells",
                 artist="AC/DC", album="Back In Black", number=1),
            play(2, 1, 220_000, 20_000, duration_ms=317_000, title="Shoot to Thrill",
                 artist="AC/DC", album="Back In Black", number=2),
            play(3, 1, 250_000, 150_000, duration_ms=200_000, title="Tab\tName",
                 artist="Sigur R\u00f3s", album="", number=0),
            play(4, 1, 410_000, 100_000, start_at=0, end_at=100_000, **long_song),
            play(5, 2, 5_000, 150_000, start_at=100_500, end_at=250_500, **long_song),
            timed(mark(6, 2, 200_000), SECOND_BOOT_MS),
            timed(play(7, 3, 1_000, 60_000, start_at=0, end_at=60_000, **paused),
                  THIRD_BOOT_MS),
            timed(mark(8, 3, 61_500), THIRD_BOOT_MS),
            timed(play(9, 3, 120_000, 100_000, start_at=62_000, end_at=162_000, **paused),
                  THIRD_BOOT_MS),
            timed(play(10, 3, 220_000, 25_000, duration_ms=25_000, title="Intro",
                       artist="AC/DC", album="Live", number=1), THIRD_BOOT_MS),
            timed(mark(11, 3, 250_000), THIRD_BOOT_MS)]


def ring(records, slots=16):
    data = bytearray(slots * echo_history.SLOT_BYTES)
    for record in records:
        at = (record.sequence - 1) % slots * echo_history.SLOT_BYTES
        data[at:at + echo_history.SLOT_BYTES] = echo_history.pack_slot(record)
    return bytes(data)


class HistoryTest(unittest.TestCase):
    def make_card(self, directory, records, slots=16):
        root = Path(directory)
        (root / ".echo").mkdir(exist_ok=True)
        (root / ".echo" / "history.bin").write_bytes(ring(records, slots))
        return root

    def run_tool(self, root, *options):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(0, echo_history.main([str(root), "--now", str(NOW), *options]))
        return output.getvalue()

    def log_lines(self, root):
        return (root / ".scrobbler.log").read_text(encoding="utf-8").splitlines(keepends=True)

    def test_a_slot_matches_the_players_layout(self):
        record = play(9, 3, 1000, 200_000, number=4, title="Ünïcode")
        slot = echo_history.pack_slot(record)
        self.assertEqual(512, len(slot))
        self.assertEqual(b"EHST", slot[:4])
        self.assertEqual(9, slot[4])
        self.assertEqual(3, slot[8])
        self.assertEqual(PLAY, slot[12])
        self.assertEqual(4, slot[13])
        self.assertEqual(b"Artist\0Album\0\xc3\x9cn\xc3\xafcode\0", slot[44:44 + 23])
        self.assertEqual(record, echo_history.unpack_slot(slot))

    def test_damaged_and_misplaced_records_are_skipped(self):
        records = [play(1, 1, 0, 200_000), play(2, 1, 300_000, 200_000)]
        data = bytearray(ring(records))
        data[512 + 100] ^= 1
        self.assertEqual([1], [record.sequence for record in echo_history.read_history(data)])
        stray = ring([play(7, 1, 0, 1000)])
        moved = stray[6 * 512:7 * 512] + bytes(15 * 512)
        self.assertEqual([], echo_history.read_history(moved))

    def test_only_the_newest_lap_of_the_ring_is_read(self):
        records = [play(sequence, 1, sequence * MINUTE_MS, 1000) for sequence in range(1, 7)]
        data = ring(records, slots=4)
        found = echo_history.read_history(data)
        self.assertEqual([3, 4, 5, 6], [record.sequence for record in found])

    def test_the_log_lines_are_rockbox_format(self):
        records = [play(1, 1, 10_000, 200_000, title="Hells Bells", artist="AC/DC",
                        album="Back In Black", number=1, duration_ms=312_400),
                   play(2, 1, 220_000, 20_000, title="Shoot to Thrill", artist="AC/DC",
                        album="Back In Black", number=2, duration_ms=317_000),
                   mark(3, 1, 300_000)]
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_card(directory, records)
            self.run_tool(root, "--include-skipped")
            lines = self.log_lines(root)
        version = echo_history.firmware_version()
        self.assertEqual(["#AUDIOSCROBBLER/1.1\n", "#TZ/UTC\n",
                          f"#CLIENT/EchoPod {version}\n".replace(" \n", "\n"),
                          f"AC/DC\tBack In Black\tHells Bells\t1\t312\tL\t{NOW - 290}\t\n",
                          f"AC/DC\tBack In Black\tShoot to Thrill\t2\t317\tS\t{NOW - 80}\t\n"],
                         lines)

    def test_the_listening_rule_is_last_fms(self):
        self.assertTrue(echo_history.listened(240_000, 120_000))
        self.assertFalse(echo_history.listened(240_000, 119_999))
        self.assertTrue(echo_history.listened(600_000, 240_000))
        self.assertFalse(echo_history.listened(600_000, 239_999))
        self.assertFalse(echo_history.listened(30_000, 30_000))
        self.assertTrue(echo_history.listened(31_000, 15_500))
        self.assertFalse(echo_history.listened(0, 500_000))

    def test_skipped_plays_are_left_out_unless_asked_for(self):
        records = [play(1, 1, 0, 10_000, title="Skipped"), play(2, 1, 20_000, 200_000)]
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_card(directory, records)
            summary = self.run_tool(root)
            lines = self.log_lines(root)
        self.assertEqual(4, len(lines))
        self.assertIn("\tSong\t", lines[3])
        self.assertIn("Left out 1 skipped", summary)

    def test_earlier_sessions_are_stacked_before_the_later_ones(self):
        records = [play(1, 1, 50_000, 200_000, title="First"),
                   play(2, 1, 300_000, 200_000, title="Second"),
                   play(3, 2, 30_000, 200_000, title="Third"),
                   mark(4, 2, 600_000)]
        plays = echo_history.plays_of(records, NOW * 1000)
        third_boot = NOW * 1000 - 600_000
        self.assertEqual((third_boot + 30_000) // 1000, plays[2].timestamp)
        first_boot = third_boot - 500_000
        self.assertEqual((first_boot + 300_000) // 1000, plays[1].timestamp)
        self.assertEqual((first_boot + 50_000) // 1000, plays[0].timestamp)

    def test_a_clock_time_dates_its_session_exactly(self):
        records = [play(1, 1, 50_000, 200_000, title="Clocked", unix_time=1_600_000_250),
                   play(2, 2, 10_000, 200_000, title="Later")]
        plays = echo_history.plays_of(records, NOW * 1000)
        self.assertEqual(1_600_000_050, plays[0].timestamp)
        self.assertEqual(NOW - 200, plays[1].timestamp)

    def test_records_before_the_clock_was_set_share_their_sessions_time(self):
        records = [play(1, 1, 50_000, 200_000, title="Before sync"),
                   mark(2, 1, 300_000, ),
                   play(3, 1, 400_000, 200_000, title="After sync", unix_time=1_600_000_600),
                   play(4, 2, 10_000, 200_000, title="Next boot")]
        records[1] = mark(2, 1, 300_000)
        plays = echo_history.plays_of(records, NOW * 1000)
        self.assertEqual(1_600_000_600 - 600 + 50, plays[0].timestamp)
        self.assertEqual(1_600_000_600 - 600 + 400, plays[1].timestamp)
        self.assertEqual(NOW - 200, plays[2].timestamp)

    def test_a_play_cut_by_a_power_off_and_resumed_counts_once(self):
        records = [play(1, 1, 0, 100_000, start_at=0, end_at=100_000),
                   play(2, 2, 5_000, 100_000, start_at=100_500, end_at=200_000),
                   play(3, 2, 200_000, 100_000, start_at=0, end_at=100_000)]
        plays = echo_history.plays_of(records, NOW * 1000)
        self.assertEqual(["L", "S"], [entry.rating for entry in plays])
        self.assertEqual(2, plays[0].sequence)
        self.assertEqual(200_000, plays[0].played_ms)
        self.assertEqual(NOW - 400, plays[0].timestamp)

    def test_a_resumed_play_after_a_listened_one_is_not_scrobbled_twice(self):
        records = [play(1, 1, 0, 150_000, start_at=0, end_at=150_000),
                   play(2, 1, 400_000, 50_000, start_at=150_000, end_at=200_000)]
        plays = echo_history.plays_of(records, NOW * 1000)
        self.assertEqual([("L", 1)], [(entry.rating, entry.sequence) for entry in plays])

    def test_running_twice_adds_only_new_plays(self):
        first = [play(1, 1, 0, 200_000, title="One"), mark(2, 1, 250_000)]
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_card(directory, first)
            self.run_tool(root)
            self.run_tool(root)
            self.assertEqual(4, len(self.log_lines(root)))
            later = first + [play(3, 2, 0, 200_000, title="Two"), mark(4, 2, 210_000)]
            self.make_card(directory, later)
            summary = self.run_tool(root)
            lines = self.log_lines(root)
            self.assertEqual(b"0000000004\n",
                             (root / ".echo" / "history-exported.txt").read_bytes())
        self.assertEqual(5, len(lines))
        self.assertEqual(1, sum(line.startswith("#AUDIOSCROBBLER") for line in lines))
        self.assertIn("\tTwo\t", lines[4])
        self.assertIn("Wrote 1 listened", summary)

    def test_an_existing_log_is_added_to(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_card(directory, [play(1, 1, 0, 200_000)])
            existing = "#AUDIOSCROBBLER/1.1\n#TZ/UTC\n#CLIENT/Other\nA\tB\tC\t\t100\tL\t5"
            (root / ".scrobbler.log").write_text(existing, encoding="utf-8", newline="\n")
            self.run_tool(root)
            lines = self.log_lines(root)
        self.assertEqual("A\tB\tC\t\t100\tL\t5\n", lines[3])
        self.assertIn("\tSong\t", lines[4])

    def test_a_recreated_history_file_starts_counting_again(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_card(directory, [play(1, 1, 0, 200_000)])
            (root / ".echo" / "history-exported.txt").write_text("900\n")
            self.run_tool(root)
            self.assertEqual(4, len(self.log_lines(root)))

    def test_the_output_can_go_elsewhere_and_json_writes_nothing(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_card(directory, [play(1, 1, 0, 200_000, title="Elsewhere")])
            listing = json.loads(self.run_tool(root, "--json"))
            self.assertEqual("Elsewhere", listing[0]["title"])
            self.assertFalse((root / ".echo" / "history-exported.txt").exists())
            target = Path(directory) / "out.log"
            self.run_tool(root, "--output", str(target))
            self.assertTrue(target.is_file())
            self.assertFalse((root / ".scrobbler.log").exists())

    def test_tabs_in_names_do_not_break_the_columns(self):
        line = echo_history.log_line(echo_history.Play(1, "A\tB", "C", "D\nE", 0, 200, "L", 7,
                                                       200_000))
        self.assertEqual("A B\tC\tD E\t\t200\tL\t7\t\n", line)

    def shared_export(self, directory, marker=None, existing=None):
        root = self.make_card(directory, shared_records())
        if marker is not None:
            (root / ".echo" / "history-exported.txt").write_bytes(marker)
        if existing is not None:
            (root / ".scrobbler.log").write_bytes(existing.encode("utf-8"))
        echo_history.export(root, now_ms=NOW * 1000, include_skipped=True, version="test")
        return root

    def test_the_shared_fixture_is_the_one_the_player_reads(self):
        self.assertEqual(ring(shared_records()), (SHARED_DIRECTORY / "history.bin").read_bytes())
        self.assertEqual(EXISTING_LOG.encode("utf-8"),
                         (SHARED_DIRECTORY / "existing.log").read_bytes())

    def test_the_player_and_the_tool_write_the_same_log(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.shared_export(directory)
            written = (root / ".scrobbler.log").read_bytes()
            marker = (root / ".echo" / "history-exported.txt").read_bytes()
        self.assertEqual((SHARED_DIRECTORY / "all.log").read_bytes(), written)
        self.assertEqual(b"0000000011\n", marker)

    def test_a_play_resumed_after_the_last_export_is_dated_from_its_start(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.shared_export(directory, b"0000000008\n", EXISTING_LOG)
            written = (root / ".scrobbler.log").read_bytes()
        self.assertEqual((SHARED_DIRECTORY / "appended.log").read_bytes(), written)

    def test_the_marker_the_player_writes_is_read(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_card(directory, [play(1, 1, 0, 200_000, title="Old"),
                                              play(2, 1, 300_000, 200_000, title="New")])
            marker = root / ".echo" / "history-exported.txt"
            for text in (b"0000000001\n", b"1\r\n", b"0000000001\n\n"):
                marker.write_bytes(text)
                (root / ".scrobbler.log").unlink(missing_ok=True)
                self.run_tool(root)
                lines = self.log_lines(root)
                self.assertEqual(4, len(lines))
                self.assertIn("\tNew\t", lines[3])
                self.assertEqual(b"0000000002\n", marker.read_bytes())

    def test_a_card_without_history_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            errors = io.StringIO()
            with contextlib.redirect_stderr(errors):
                self.assertEqual(1, echo_history.main([directory]))
        self.assertIn("no listen history", errors.getvalue())


if __name__ == "__main__":
    unittest.main()

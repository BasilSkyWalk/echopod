import contextlib
import io
import struct
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIRECTORY))

import echo_bookmarks


def bookmarks_file(records):
    body = echo_bookmarks.HEADER.pack(b"EMBK", 1, len(records), 0)
    for key, track_key, position_ms, chapter, finished in records:
        body += echo_bookmarks.RECORD.pack(key, track_key, position_ms, chapter,
                                           1 if finished else 0, 0)
    return body + struct.pack("<I", echo_bookmarks.fnv1a(body))


def key(path):
    return echo_bookmarks.fnv1a(path.encode("utf-8"))


class BookmarksTest(unittest.TestCase):
    def make_card(self, directory, records):
        root = Path(directory)
        for relative in ("Podcasts/Show/Episode 1.mp3", "Podcasts/Show/Episode 2.mp3",
                         "Audiobooks/The Long Road/01.mp3"):
            (root / relative).parent.mkdir(parents=True, exist_ok=True)
            (root / relative).write_bytes(b"")
        (root / ".echo").mkdir()
        (root / ".echo" / "bookmarks.bin").write_bytes(bookmarks_file(records) + bytes(64))
        return root

    def run_tool(self, root, *options):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(0, echo_bookmarks.main([str(root), *options]))
        return output.getvalue()

    def test_a_damaged_file_reads_as_no_bookmarks(self):
        data = bytearray(bookmarks_file([(1, 2, 3, 0, False)]))
        data[-1] ^= 1
        self.assertEqual([], echo_bookmarks.read_bookmarks(bytes(data)))
        self.assertEqual([], echo_bookmarks.read_bookmarks(bytes(4096)))

    def test_finished_episodes_and_books_are_listed_by_path(self):
        episode = "Podcasts/Show/Episode 1.mp3"
        book = "Audiobooks/The Long Road"
        records = [(key(episode), key(episode), 0, 0, True),
                   (key("Podcasts/Show/Episode 2.mp3"), key("Podcasts/Show/Episode 2.mp3"),
                    61000, 1, False),
                   (key(book), key(book + "/01.mp3"), 0, 0, True)]
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_card(directory, records)
            finished = self.run_tool(root, "--finished").splitlines()
            listing = self.run_tool(root)
        self.assertEqual([episode, book], finished)
        self.assertIn("0:01:01  Podcasts/Show/Episode 2.mp3", listing)


if __name__ == "__main__":
    unittest.main()

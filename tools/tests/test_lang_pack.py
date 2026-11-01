import struct
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIRECTORY))

import lang_pack


def write_tree(root, definition, languages):
    catalogue = Path(root) / "catalogue"
    lang = Path(root) / "lang"
    catalogue.mkdir()
    lang.mkdir()
    (catalogue / "strings_all.def").write_text('#include "core/strings_one.def"\n', encoding="utf-8")
    (catalogue / "strings_one.def").write_text(definition, encoding="utf-8")
    (lang / "languages.json").write_text(
        '{"languages": [{"code": "en", "name": "English"}, {"code": "de", "name": "German"}]}',
        encoding="utf-8")
    (lang / "de.txt").write_text(languages, encoding="utf-8")
    return catalogue, lang


class CatalogueTest(unittest.TestCase):
    def test_entries_come_in_include_order_with_escapes_decoded(self):
        with tempfile.TemporaryDirectory() as root:
            definition = 'STRING(STR_A, "One")\nSTRING(STR_B, "x\\xC2\\xB7y")\n'
            catalogue, _ = write_tree(root, definition, "")
            self.assertEqual([("STR_A", "One"), ("STR_B", "x·y")],
                             lang_pack.read_catalogue(catalogue))

    def test_a_duplicate_id_is_an_error(self):
        with tempfile.TemporaryDirectory() as root:
            definition = 'STRING(STR_A, "One")\nSTRING(STR_A, "Two")\n'
            catalogue, _ = write_tree(root, definition, "")
            with self.assertRaises(lang_pack.PackError):
                lang_pack.read_catalogue(catalogue)


class PackTest(unittest.TestCase):
    def build(self, translations):
        with tempfile.TemporaryDirectory() as root:
            definition = 'STRING(STR_ON, "On")\nSTRING(STR_TIMES, "{0} of {1}")\n'
            catalogue, lang = write_tree(root, definition, translations)
            return lang_pack.build_all(catalogue, lang)

    def test_a_pack_holds_every_string_in_order_and_empty_means_english(self):
        packs, coverage = self.build("STR_ON = Ein\n")
        pack = packs["de"]
        magic, version, count, size = lang_pack.HEADER.unpack_from(pack)
        self.assertEqual((b"ELNG", 1, 2, len(pack)), (magic, version, count, size))
        offsets = struct.unpack_from("<2H", pack, lang_pack.HEADER.size)
        base = lang_pack.HEADER.size + 4
        self.assertEqual(b"Ein\0", pack[base + offsets[0]:base + offsets[0] + 4])
        self.assertEqual(b"{0} of {1}\0", pack[base + offsets[1]:base + offsets[1] + 11])
        self.assertEqual((1, 2), coverage["de"])

    def test_english_has_a_complete_pack_of_its_own_text(self):
        packs, _ = self.build("")
        self.assertEqual(["en", "de"], list(packs))
        self.assertIn(b"On\0{0} of {1}\0", packs["en"])

    def test_an_unknown_id_is_an_error(self):
        with self.assertRaises(lang_pack.PackError):
            self.build("STR_MISSING = x\n")

    def test_a_repeated_translation_is_an_error(self):
        with self.assertRaises(lang_pack.PackError):
            self.build("STR_ON = a\nSTR_ON = b\n")

    def test_placeholders_must_match_the_english_text(self):
        with self.assertRaises(lang_pack.PackError):
            self.build("STR_TIMES = {0} von\n")
        packs, _ = self.build("STR_TIMES = {1} von {0}\n")
        self.assertIn(b"{1} von {0}", packs["de"])

    def test_comments_and_blank_lines_are_ignored(self):
        packs, _ = self.build("; note\n\nSTR_ON = Ein\n")
        self.assertIn(b"Ein", packs["de"])

    def test_a_character_no_glyph_pack_draws_is_an_error(self):
        with self.assertRaises(lang_pack.PackError):
            self.build("STR_ON = ال\n")
        packs, _ = self.build("STR_ON = Конец 中文 ü\n")
        self.assertIn("中文".encode("utf-8"), packs["de"])

    def test_an_oversized_pack_is_an_error(self):
        with self.assertRaises(lang_pack.PackError):
            self.build("STR_ON = " + "x" * lang_pack.PACK_LIMIT + "\n")


class CommittedPacksTest(unittest.TestCase):
    def test_every_committed_translation_builds_and_is_current(self):
        root = TOOLS_DIRECTORY.parent
        packs, _ = lang_pack.build_all(root / "src" / "core" / "include" / "core",
                                       root / "assets" / "lang")
        self.assertEqual(["en"], list(packs)[:1])
        for code, pack in packs.items():
            committed = root / "src" / "lang" / "packs" / f"{code}.bin"
            self.assertTrue(committed.exists(), code)
            self.assertEqual(committed.read_bytes(), pack, code)


if __name__ == "__main__":
    unittest.main()

import json
import struct
import sys
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
ASSETS_DIRECTORY = TOOLS_DIRECTORY.parent / "assets" / "fonts"
sys.path.insert(0, str(TOOLS_DIRECTORY))

import font_build
import glyph_pack


def glyph(codepoint, width=2, height=2, left=0, top=2, advance=3, levels=None):
    levels = levels if levels is not None else [15] * (width * height)
    return font_build.GlyphImage(codepoint, width, height, left, top, advance, levels)


class CharacterSetTest(unittest.TestCase):
    def test_ks_x_1001_has_2350_hangul_syllables(self):
        spec = {"codec": "euc_kr", "leads": [176, 200], "ranges": [[0xAC00, 0xD7A3]]}
        hangul = glyph_pack.set_characters(spec)
        self.assertEqual(2350, len(hangul))
        self.assertIn(ord("한"), hangul)
        self.assertIn(ord("글"), hangul)

    def test_gb_2312_has_6763_hanzi(self):
        spec = {"codec": "gb2312", "leads": [176, 247], "ranges": [[0x4E00, 0x9FFF]]}
        hanzi = glyph_pack.set_characters(spec)
        self.assertEqual(6763, len(hanzi))
        self.assertIn(ord("中"), hanzi)

    def test_ranges_without_a_codec_are_taken_whole(self):
        self.assertEqual({0x3041, 0x3042, 0x3043},
                         glyph_pack.set_characters({"ranges": [[0x3041, 0x3043]]}))

    def test_the_cjk_faces_cover_korean_chinese_and_japanese(self):
        config = json.loads((ASSETS_DIRECTORY / "glyph_pack.json").read_text())
        for face in config["faces"][:2]:
            wanted = glyph_pack.face_characters(config, face)
            for character in "한국어中文日本語ひらがなカタカナ。「」！":
                self.assertIn(ord(character), wanted, character)
            self.assertLess(len(wanted), 13000)

    def test_the_latin1_faces_hold_the_accented_letters(self):
        config = json.loads((ASSETS_DIRECTORY / "glyph_pack.json").read_text())
        for face in config["faces"][2:]:
            self.assertEqual(set(range(160, 256)), glyph_pack.face_characters(config, face))

    def test_faces_are_numbered_from_one_in_config_order(self):
        config = {"faces": [{"name": "a"}, {"name": "b"}]}
        self.assertEqual({"a": 1, "b": 2}, glyph_pack.face_numbers(config))


class PackTest(unittest.TestCase):
    def test_a_packed_face_reads_back(self):
        glyphs = [glyph(0x4E00 + index, levels=[index % 16, 15, 0, 7]) for index in range(100)]
        blob = glyph_pack.pack_glyphs(glyphs)
        read = glyph_pack.read_face(blob)
        self.assertEqual(100, len(read))
        self.assertEqual((2, 2, 0, 2, 3, bytes([0x5F, 0x07])), read[0x4E05])

    def test_the_header_matches_the_player_reader(self):
        blob = glyph_pack.pack_glyphs([glyph(0x4E00 + index) for index in range(50)])
        magic, version, entry, per_sector = struct.unpack_from("<4sBBH", blob)
        count, directory, index = struct.unpack_from("<III", blob, 8)
        self.assertEqual((b"EGLF", 1, 12, 42), (magic, version, entry, per_sector))
        self.assertEqual((50, 512, 1024), (count, directory, index))
        self.assertEqual(0x4E00 + 42, struct.unpack_from("<H", blob, directory + 2)[0])
        self.assertEqual(0x4E00 + 42, struct.unpack_from("<H", blob, index + 512)[0])

    def test_entries_fill_whole_sectors(self):
        self.assertEqual(42, glyph_pack.ENTRIES_PER_SECTOR)
        self.assertEqual(12, glyph_pack.ENTRY.size)

    def test_a_blank_glyph_has_no_bitmap(self):
        blob = glyph_pack.pack_glyphs([glyph(0x3000, 0, 0, levels=[]), glyph(0x3001)])
        read = glyph_pack.read_face(blob)
        self.assertEqual(b"", read[0x3000][5])
        self.assertEqual(bytes([0xFF, 0xFF]), read[0x3001][5])

    def test_a_corrupted_face_is_refused(self):
        blob = bytearray(glyph_pack.pack_glyphs([glyph(0x4E00)]))
        blob[-1] ^= 1
        with self.assertRaises(glyph_pack.PackError):
            glyph_pack.read_face(bytes(blob))

    def test_an_empty_face_is_refused(self):
        with self.assertRaises(glyph_pack.PackError):
            glyph_pack.pack_glyphs([])

    def test_too_many_glyphs_are_refused(self):
        count = glyph_pack.MAX_DIRECTORY * glyph_pack.ENTRIES_PER_SECTOR + 1
        with self.assertRaises(glyph_pack.PackError):
            glyph_pack.pack_glyphs([glyph(index, 0, 0, levels=[]) for index in range(count)])

    def test_metrics_out_of_range_are_refused(self):
        with self.assertRaises(glyph_pack.PackError):
            glyph_pack.pack_glyphs([glyph(0x4E00, left=200)])


class BuildFaceTest(unittest.TestCase):
    def test_only_characters_in_the_font_are_packed(self):
        spec = {"name": "latin", "file": "inter/Inter-Regular.ttf", "size": 16}
        face = glyph_pack.build_face(spec, {ord("A"), ord("B"), 0x4E00}, ASSETS_DIRECTORY)
        read = glyph_pack.read_face(face.blob)
        self.assertEqual({ord("A"), ord("B")}, set(read))
        self.assertEqual(2, face.glyph_count)
        self.assertEqual([], face.dropped)

    def test_a_face_without_its_font_file_is_skipped(self):
        spec = {"name": "missing", "file": "nowhere/Missing.otf", "size": 16}
        self.assertIsNone(glyph_pack.build_face(spec, {ord("A")}, ASSETS_DIRECTORY))

    def test_rendering_matches_the_built_in_fonts(self):
        spec = {"name": "latin", "file": "inter/Inter-Regular.ttf", "size": 16}
        face = glyph_pack.build_face(spec, {ord("H")}, ASSETS_DIRECTORY)
        font = font_build.load_font(ASSETS_DIRECTORY / spec["file"], 16)
        expected = font_build.render_glyph(font, ord("H"))
        width, height, left, top, advance, bitmap = glyph_pack.read_face(face.blob)[ord("H")]
        self.assertEqual((expected.width, expected.height, expected.left, expected.top,
                          expected.advance), (width, height, left, top, advance))
        self.assertEqual(font_build.pack_nibbles(expected.levels), bitmap)


if __name__ == "__main__":
    unittest.main()

import sys
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
ASSETS_DIRECTORY = TOOLS_DIRECTORY.parent / "assets" / "fonts"
sys.path.insert(0, str(TOOLS_DIRECTORY))

import font_build


class QuantizeAlphaTest(unittest.TestCase):
    def test_transparent_stays_zero(self):
        self.assertEqual(0, font_build.quantize_alpha(0))

    def test_opaque_becomes_fifteen(self):
        self.assertEqual(15, font_build.quantize_alpha(255))

    def test_midpoint_rounds_to_nearest_level(self):
        self.assertEqual(8, font_build.quantize_alpha(128))
        self.assertEqual(1, font_build.quantize_alpha(9))


class PackNibblesTest(unittest.TestCase):
    def test_first_level_goes_in_the_high_nibble(self):
        self.assertEqual(bytes([0x12, 0x34]), font_build.pack_nibbles([1, 2, 3, 4]))

    def test_odd_count_pads_the_last_low_nibble_with_zero(self):
        self.assertEqual(bytes([0x12, 0x30]), font_build.pack_nibbles([1, 2, 3]))

    def test_no_levels_pack_to_nothing(self):
        self.assertEqual(b"", font_build.pack_nibbles([]))


class RenderGlyphTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.font = font_build.load_font(ASSETS_DIRECTORY / "inter" / "Inter-Regular.ttf", 16)

    def test_a_letter_has_ink_above_the_baseline(self):
        glyph = font_build.render_glyph(self.font, ord("H"))
        self.assertGreater(glyph.width, 0)
        self.assertGreater(glyph.height, 0)
        self.assertGreater(glyph.top, 0)
        self.assertEqual(glyph.width * glyph.height, len(glyph.levels))
        self.assertEqual(15, max(glyph.levels))

    def test_a_space_has_an_advance_but_no_ink(self):
        glyph = font_build.render_glyph(self.font, ord(" "))
        self.assertEqual(0, glyph.width * glyph.height)
        self.assertGreater(glyph.advance, 0)

    def test_a_descender_reaches_below_the_baseline(self):
        glyph = font_build.render_glyph(self.font, ord("g"))
        self.assertGreater(glyph.height, glyph.top)


    def test_glyphs_are_cropped_to_their_visible_ink(self):
        for character in '"-!H':
            glyph = font_build.render_glyph(self.font, ord(character))
            rows = [glyph.levels[row * glyph.width:(row + 1) * glyph.width]
                    for row in range(glyph.height)]
            columns = [glyph.levels[column::glyph.width] for column in range(glyph.width)]
            self.assertTrue(any(rows[0]) and any(rows[-1]), character)
            self.assertTrue(any(columns[0]) and any(columns[-1]), character)

    def test_cropping_keeps_the_ink_in_place(self):
        glyph = font_build.render_glyph(self.font, ord('"'))
        self.assertGreater(glyph.top, glyph.height)
        self.assertGreaterEqual(glyph.left, 0)


class CoverageTest(unittest.TestCase):
    def test_only_codepoints_in_the_font_are_kept(self):
        path = ASSETS_DIRECTORY / "inter" / "Inter-Regular.ttf"
        covered = font_build.covered_codepoints(path, [[65, 66], [0x4E00, 0x4E01]])
        self.assertEqual([65, 66], covered)


class SourceTest(unittest.TestCase):
    def test_font_source_lists_glyphs_in_codepoint_order(self):
        glyphs = [
            font_build.GlyphImage(66, 1, 1, 0, 1, 2, [15]),
            font_build.GlyphImage(65, 1, 1, 0, 1, 2, [15]),
        ]
        font = font_build.FontData("tiny", 3, 1, glyphs)
        source = font_build.font_source(font)
        self.assertLess(source.index("{65,"), source.index("{66,"))
        self.assertIn("const Font FONT_TINY = {", source)
        self.assertIn(".glyph_count = 2,", source)
        self.assertIn(".bitmap_size = 2,", source)

    def test_header_declares_every_font(self):
        header = font_build.header_source(["inter_regular_12", "inter_semibold_16"])
        self.assertIn("extern const Font FONT_INTER_REGULAR_12;", header)
        self.assertIn("extern const Font FONT_INTER_SEMIBOLD_16;", header)


class OutputComparisonTest(unittest.TestCase):
    def test_identical_outputs_match(self):
        outputs = {"fonts.c": "a", "include/fonts/fonts.h": "b"}
        self.assertEqual([], font_build.stale_outputs(outputs, dict(outputs)))

    def test_changed_or_missing_outputs_are_reported(self):
        generated = {"fonts.c": "new", "include/fonts/fonts.h": "b"}
        committed = {"fonts.c": "old"}
        self.assertEqual(["fonts.c", "include/fonts/fonts.h"],
                         font_build.stale_outputs(generated, committed))


if __name__ == "__main__":
    unittest.main()

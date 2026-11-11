import struct
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIRECTORY))

import theme_pack

COLORS = ["background", "text", "accent"]
BOXES = ["selection", "header"]
BASE = {
    "name": "Base",
    "colors": {"background": "#000000", "text": "#ffffff", "accent": "#ff0000"},
    "boxes": {"selection": {"fill": "accent", "ink": "text"}, "header": {"fill": "background"}},
}
HEADER = struct.Struct("<4sHHIIHHBBBB24s")
SPACING = [("row_height", 18, 40), ("padding", 4, 20), ("scrollbar_inset", 0, 6)]


def packed(theme, base=None):
    return theme_pack.pack(theme, BASE if base is None else base, COLORS, BOXES)


def header(package):
    return HEADER.unpack_from(package)


def colors_of(package):
    return struct.unpack_from("<3H", package, theme_pack.HEADER_SIZE)


def box(package, index):
    start = theme_pack.HEADER_SIZE + 2 * len(COLORS) + index * theme_pack.BOX_SIZE
    return tuple(package[start:start + theme_pack.BOX_SIZE])


class PackTest(unittest.TestCase):
    def test_the_header_says_what_follows_and_checks_it(self):
        package = packed({"name": "Mine"})
        (magic, version, header_size, size, check, colors, boxes, box_size, selection, spaces, zero,
         name) = header(package)
        self.assertEqual((b"ETHM", 1, 48), (magic, version, header_size))
        self.assertEqual(len(package), size)
        self.assertEqual(theme_pack.checksum(package[16:]), check)
        self.assertEqual((3, 2, 11, 0, 0, 0), (colors, boxes, box_size, selection, spaces, zero))
        self.assertEqual(b"Mine" + bytes(20), name)
        self.assertEqual(48 + 6 + 22, len(package))

    def test_the_checksum_is_the_fnv_hash_the_player_computes(self):
        self.assertEqual(0x811C9DC5, theme_pack.checksum(b""))
        self.assertEqual(0xE40C292C, theme_pack.checksum(b"a"))

    def test_colours_are_stored_as_rgb565_in_definition_order(self):
        package = packed({"name": "Mine", "colors": {"text": "#101114", "accent": "#f2f2f5"}})
        self.assertEqual((0x0000, 0x1082, 0xF79E), colors_of(package))

    def test_what_a_theme_leaves_out_comes_from_the_base(self):
        package = packed({"name": "Mine", "colors": {"accent": "#0000ff"}})
        self.assertEqual((0x0000, 0xFFFF, 0x001F), colors_of(package))
        self.assertEqual((2, 2, 0, 0, 1, 0, 0, 0, 0, 0, 0), box(package, 0))

    def test_a_box_holds_colour_ids_and_its_measures(self):
        package = packed({"name": "Mine", "boxes": {"header": {
            "fill": ["accent", "background"], "border": "text", "border_width": 2,
            "border_edges": ["bottom", "left"], "shine": "accent", "shine_height": 1,
            "ink": "text", "inset": [6, 1], "radius": 5}}})
        self.assertEqual((2, 0, 1, 2, 1, 2, 2 | 4, 1, 6, 1, 5), box(package, 1))

    def test_a_border_with_a_width_goes_all_round_unless_edges_are_named(self):
        package = packed({"name": "Mine", "boxes": {"header": {"border": "text", "border_width": 1}}})
        self.assertEqual(15, box(package, 1)[6])
        plain = packed({"name": "Mine", "boxes": {"header": {"fill": "text"}}})
        self.assertEqual(0, box(plain, 1)[6])

    def test_the_selection_names_the_box_that_draws_the_selected_row(self):
        self.assertEqual(1, header(packed({"name": "Mine", "selection": "header"}))[8])
        with self.assertRaises(theme_pack.ThemeError):
            packed({"name": "Mine", "selection": "footer"})

    def test_unknown_names_and_bad_values_are_refused(self):
        for theme in ({"name": "Mine", "colors": {"sky": "#000000"}},
                      {"name": "Mine", "colors": {"text": "white"}},
                      {"name": "Mine", "colors": {"text": "#fff"}},
                      {"name": "Mine", "boxes": {"footer": {}}},
                      {"name": "Mine", "boxes": {"header": {"fill": "sky"}}},
                      {"name": "Mine", "boxes": {"header": {"radius": 33}}},
                      {"name": "Mine", "boxes": {"header": {"corner": 4}}},
                      {"name": "Mine", "spacing": {"row_height": 28}},
                      {"name": "Mine", "boxes": {"header": {"border_width": 256}}},
                      {"name": "Mine", "boxes": {"header": {"border_edges": ["middle"]}}},
                      {"name": "Mine", "boxes": {"header": {"inset": [1]}}},
                      {"name": "Mine", "wallpaper": "sky.png"}):
            with self.assertRaises(theme_pack.ThemeError):
                packed(theme)

    def test_a_theme_needs_a_name_that_fits(self):
        with self.assertRaises(theme_pack.ThemeError):
            packed({})
        with self.assertRaises(theme_pack.ThemeError):
            packed({"name": "x" * 24})
        self.assertEqual(b"x" * 23 + b"\x00", header(packed({"name": "x" * 23}))[11])

    def test_spacing_is_stored_one_up_so_zero_means_the_player_default(self):
        package = theme_pack.pack({"name": "Mine", "spacing": {"row_height": 28, "scrollbar_inset": 0}},
                                  BASE, COLORS, BOXES, spacing=SPACING)
        self.assertEqual(3, header(package)[9])
        self.assertEqual(bytes([29, 0, 1]), package[-3:])
        self.assertEqual(len(package), header(package)[3])

    def test_spacing_outside_its_range_or_unknown_is_refused(self):
        for spacing in ({"row_height": 17}, {"row_height": 41}, {"padding": 4.5},
                        {"margin": 4}, {"row_height": True}):
            with self.assertRaises(theme_pack.ThemeError):
                theme_pack.pack({"name": "Mine", "spacing": spacing}, BASE, COLORS, BOXES,
                                spacing=SPACING)

    def test_the_base_itself_must_define_every_colour_and_box(self):
        with self.assertRaises(theme_pack.ThemeError):
            packed({"name": "Base", "colors": {"text": "#ffffff"}}, base={})


class SourcesTest(unittest.TestCase):
    def write_sources(self, root):
        sources = Path(root) / "themes"
        definitions = Path(root) / "definitions"
        sources.mkdir()
        definitions.mkdir()
        (definitions / "theme_colors.def").write_text(
            "THEME_COLOR(BACKGROUND, background)\nTHEME_COLOR(TEXT, text)\n", encoding="utf-8")
        (definitions / "theme_boxes.def").write_text("THEME_BOX(SELECTION, selection)\n",
                                                     encoding="utf-8")
        (definitions / "theme_spacing.def").write_text(
            "THEME_SPACING(ROW_HEIGHT, row_height, 18, 40)\n", encoding="utf-8")
        (sources / "dark.toml").write_text(
            'name = "Dark"\n[colors]\nbackground = "#000000"\ntext = "#ffffff"\n'
            '[boxes.selection]\nfill = "text"\n', encoding="utf-8")
        (sources / "paper.toml").write_text('name = "Paper"\n[colors]\nbackground = "#ffffff"\n',
                                            encoding="utf-8")
        return sources, definitions

    def test_every_source_is_packed_over_the_dark_base(self):
        with tempfile.TemporaryDirectory() as root:
            sources, definitions = self.write_sources(root)
            packages = theme_pack.build_all(sources, definitions)
            self.assertEqual(["dark", "paper"], sorted(packages))
            self.assertEqual((0xFFFF, 0xFFFF),
                             struct.unpack_from("<2H", packages["paper"], theme_pack.HEADER_SIZE))

    def test_check_fails_when_a_packed_theme_is_missing_or_stale(self):
        with tempfile.TemporaryDirectory() as root:
            sources, definitions = self.write_sources(root)
            output = Path(root) / "out"
            arguments = ["--themes", str(sources), "--definitions", str(definitions),
                         "--output", str(output)]
            self.assertEqual(1, theme_pack.main(arguments + ["--check"]))
            self.assertEqual(0, theme_pack.main(arguments))
            self.assertEqual(0, theme_pack.main(arguments + ["--check"]))
            (output / "paper.echotheme").write_bytes(b"stale")
            self.assertEqual(1, theme_pack.main(arguments + ["--check"]))

    def test_a_source_that_is_not_toml_is_reported_by_name(self):
        with tempfile.TemporaryDirectory() as root:
            sources, definitions = self.write_sources(root)
            (sources / "broken.toml").write_text("name = ", encoding="utf-8")
            with self.assertRaisesRegex(theme_pack.ThemeError, "broken.toml"):
                theme_pack.build_all(sources, definitions)


if __name__ == "__main__":
    unittest.main()

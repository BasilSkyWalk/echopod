import importlib.util
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


def scene_names():
    folder = TOOLS_DIRECTORY.parent / "src" / "np_faces" / "include" / "np_faces"
    return theme_pack.scene_compile.definitions(folder)


def asset_entries(package):
    table_at = theme_pack.ASSET_HEADER_SIZE + 2 * len(COLORS) + theme_pack.BOX_SIZE * len(BOXES)
    return [struct.unpack_from("<BBHHHII", package, table_at + 16 * index) for index in range(package[48])]


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
        self.assertEqual((3, 2, 12, 0, 0, 0), (colors, boxes, box_size, selection, spaces, zero))
        self.assertEqual(b"Mine" + bytes(20), name)
        self.assertEqual(48 + 6 + 24, len(package))

    def test_the_checksum_is_the_fnv_hash_the_player_computes(self):
        self.assertEqual(0x811C9DC5, theme_pack.checksum(b""))
        self.assertEqual(0xE40C292C, theme_pack.checksum(b"a"))

    def test_colours_are_stored_as_rgb565_in_definition_order(self):
        package = packed({"name": "Mine", "colors": {"text": "#101114", "accent": "#f2f2f5"}})
        self.assertEqual((0x0000, 0x1082, 0xF79E), colors_of(package))

    def test_what_a_theme_leaves_out_comes_from_the_base(self):
        package = packed({"name": "Mine", "colors": {"accent": "#0000ff"}})
        self.assertEqual((0x0000, 0xFFFF, 0x001F), colors_of(package))
        self.assertEqual((2, 2, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0), box(package, 0))

    def test_a_box_holds_colour_ids_and_its_measures(self):
        package = packed({"name": "Mine", "boxes": {"header": {
            "fill": ["accent", "background"], "border": "text", "border_width": 2,
            "border_edges": ["bottom", "left"], "shine": "accent", "shine_height": 1,
            "ink": "text", "inset": [6, 1], "radius": 5}}})
        self.assertEqual((2, 0, 1, 2, 1, 2, 2 | 4, 1, 6, 1, 5, 0), box(package, 1))

    def test_a_theme_can_leave_out_the_home_art(self):
        self.assertEqual(2, packed({"name": "Mine", "home_art": False})[52])
        self.assertEqual(3, packed({"name": "Mine", "home_art": False, "cover_flow": "dark"})[52])
        self.assertEqual(packed({"name": "Mine"}), packed({"name": "Mine", "home_art": True}))
        with self.assertRaises(theme_pack.ThemeError):
            packed({"name": "Mine", "home_art": "no"})

    def test_a_home_scene_packs_beside_the_now_playing_one(self):
        names = scene_names()
        theme = {"name": "Mine", "now_playing_scene": {"script": "rect(0, 0, 4, 4, 0)"},
                 "home_scene": {"script": "menu(x, y, width, height)", "design": "{}"}}
        package = theme_pack.pack(theme, BASE, COLORS, BOXES, scene_names=names)
        kinds = [entry[0] for entry in asset_entries(package)]
        self.assertEqual([5, 6, 8, 9, 10], kinds)

    def test_the_two_scenes_share_one_limit(self):
        names = scene_names()
        script = "\n".join(f"rect({index}, {index}, 1000, 1000, 70000)" for index in range(260))
        half = {"name": "Mine", "now_playing_scene": {"script": script}}
        theme_pack.pack(half, BASE, COLORS, BOXES, scene_names=names)
        both = {**half, "home_scene": {"script": script}}
        with self.assertRaises(theme_pack.ThemeError):
            theme_pack.pack(both, BASE, COLORS, BOXES, scene_names=names)

    def test_opacity_is_stored_as_how_far_the_box_is_see_through(self):
        def see_through(opacity):
            return box(packed({"name": "Mine", "boxes": {"header": {"opacity": opacity}}}), 1)[11]
        self.assertEqual([0, 15, 7, 5], [see_through(100), see_through(0), see_through(50), see_through(66)])

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
                      {"name": "Mine", "boxes": {"header": {"opacity": 101}}},
                      {"name": "Mine", "boxes": {"header": {"opacity": 0.5}}},
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


    def test_the_author_and_description_are_one_short_line_each(self):
        packed({"name": "Mine", "author": "a" * 40, "description": "d" * 120})
        for credit in ({"author": "a" * 41}, {"description": "d" * 121}, {"author": 7},
                       {"description": "two\nlines"}):
            with self.assertRaises(theme_pack.ThemeError):
                packed({"name": "Mine", **credit})


TILE = {"palette": ["#000000", "#ffffff", "#ff0000"], "rows": ["0120", "2100"]}
TILE_SECTION = struct.pack("<16H", 0x0000, 0xFFFF, 0xF800, *[0] * 13) + bytes([0x01, 0x20, 0x21, 0x00])
WALL_ENTRY = struct.Struct("<BBHHHII")


def papered(theme):
    return packed({"name": "Mine", **theme})


def wall_entries(package):
    table_at = 56 + 6 + 24
    return [WALL_ENTRY.unpack_from(package, table_at + 16 * index) for index in range(package[48])]


class WallpaperTest(unittest.TestCase):
    def test_a_theme_without_a_wallpaper_keeps_the_short_header(self):
        self.assertEqual(48, header(packed({"name": "Mine"}))[2])

    def test_a_wallpaper_follows_the_checked_part_and_the_table_says_where(self):
        package = papered({"wallpaper": TILE})
        size = header(package)[3]
        self.assertEqual(56, header(package)[2])
        self.assertEqual(56 + 6 + 24 + 16, size)
        self.assertEqual(theme_pack.checksum(package[16:size]), header(package)[4])
        self.assertEqual([(0, 0, 4, 2, 0, size, theme_pack.checksum(TILE_SECTION))],
                         wall_entries(package))
        self.assertEqual(TILE_SECTION, package[size:])

    def test_the_colours_and_boxes_start_after_the_longer_header(self):
        package = papered({"wallpaper": TILE, "colors": {"accent": "#0000ff"}})
        self.assertEqual((0x0000, 0xFFFF, 0x001F), struct.unpack_from("<3H", package, 56))
        self.assertEqual([], theme_pack.lint(papered({"wallpaper": TILE, "boxes": {
            "header": {"fill": "background", "ink": "text"}}}), COLORS, BOXES)[1:])

    def test_an_upright_wallpaper_is_the_second_kind_and_lies_after_the_first(self):
        package = papered({"wallpaper": TILE, "wallpaper_portrait": TILE})
        first, second = wall_entries(package)
        self.assertEqual((0, 1), (first[0], second[0]))
        self.assertEqual(first[5] + len(TILE_SECTION), second[5])
        self.assertEqual(header(package)[3] + 2 * len(TILE_SECTION), len(package))
        alone = papered({"wallpaper_portrait": TILE})
        self.assertEqual(1, wall_entries(alone)[0][0])

    def test_a_wallpaper_comes_from_the_base_unless_the_theme_has_its_own(self):
        base = {**BASE, "wallpaper": TILE}
        self.assertEqual(TILE_SECTION, packed({"name": "Mine"}, base)[-len(TILE_SECTION):])

    def test_a_misshapen_wallpaper_is_refused(self):
        wide = {"palette": ["#000000"], "rows": ["0" * 322]}
        tall = {"palette": ["#000000"], "rows": ["00"] * 171}
        for wallpaper in ({"palette": ["#000000"], "rows": ["000"]},
                          {"palette": ["#000000"], "rows": ["00", "0000"]},
                          {"palette": ["#000000"], "rows": ["01"]},
                          {"palette": ["#000000"], "rows": ["0g"]},
                          {"palette": [], "rows": ["00"]},
                          {"palette": ["#000000"] * 17, "rows": ["00"]},
                          {"palette": ["black"], "rows": ["00"]},
                          {"palette": ["#000000"], "rows": []},
                          {"palette": ["#000000"], "rows": "00"},
                          {"palette": ["#000000"], "rows": ["00"], "tile": True},
                          {"image": "paper.png"}, wide, tall, "paper.png"):
            with self.assertRaises(theme_pack.ThemeError):
                papered({"wallpaper": wallpaper})
        papered({"wallpaper_portrait": {"palette": ["#000000"], "rows": ["00"] * 320}})
        with self.assertRaises(theme_pack.ThemeError):
            papered({"wallpaper_portrait": {"palette": ["#000000"], "rows": ["0" * 172]}})

    def test_text_is_checked_against_the_wallpaper_it_lies_on(self):
        clear = {"boxes": {"header": {"fill": "background", "ink": "text"}}}
        pale = {"palette": ["#000000", "#eeeeee"], "rows": ["0000000001"] * 10}
        self.assertEqual([], theme_pack.lint(papered({"wallpaper": pale, **clear}), COLORS, BOXES))
        paler = {"palette": ["#000000", "#eeeeee"], "rows": ["0000000011"] * 10}
        found = theme_pack.lint(papered({"wallpaper": paler, **clear}), COLORS, BOXES)
        self.assertEqual(1, len(found))
        self.assertRegex(found[0], r"^List text over the wallpaper: contrast 1\.\d, below 3\.0$")
        upright = theme_pack.lint(papered({"wallpaper": pale, "wallpaper_portrait": paler, **clear}),
                                  COLORS, BOXES)
        self.assertEqual(found, upright)


@unittest.skipUnless(importlib.util.find_spec("PIL"), "Pillow is not installed")
class WallpaperImageTest(unittest.TestCase):
    def write_theme(self, root, wallpaper, size):
        from PIL import Image
        folder = Path(root) / "themes"
        definitions = Path(root) / "definitions"
        folder.mkdir()
        definitions.mkdir()
        (definitions / "theme_colors.def").write_text(
            "THEME_COLOR(BACKGROUND, background)\nTHEME_COLOR(TEXT, text)\n", encoding="utf-8")
        (definitions / "theme_boxes.def").write_text("THEME_BOX(SELECTION, selection)\n",
                                                     encoding="utf-8")
        (definitions / "theme_spacing.def").write_text("", encoding="utf-8")
        picture = Image.new("RGB", size)
        picture.putdata([(x * 255 // size[0], y * 255 // size[1], 90)
                         for y in range(size[1]) for x in range(size[0])])
        picture.save(folder / "paper.png")
        (folder / "dark.toml").write_text(
            'name = "Dark"\n[colors]\nbackground = "#000000"\ntext = "#ffffff"\n'
            '[boxes.selection]\nfill = "text"\nink = "background"\n' + wallpaper, encoding="utf-8")
        return folder, definitions

    def entry_of(self, root, wallpaper, size):
        folder, definitions = self.write_theme(root, wallpaper, size)
        package = theme_pack.build_all(folder, definitions)["dark"]
        entry = WALL_ENTRY.unpack_from(package, 56 + 4 + 12)
        return package, entry

    def test_a_big_picture_is_fitted_to_the_screen_and_cut_to_sixteen_colours(self):
        with tempfile.TemporaryDirectory() as root:
            package, entry = self.entry_of(root, '[wallpaper]\nimage = "paper.png"\n', (640, 400))
            self.assertEqual((320, 170), entry[2:4])
            section = package[entry[5]:]
            self.assertEqual(32 + 160 * 170, len(section))
            self.assertEqual(theme_pack.checksum(section), entry[6])
            self.assertGreater(len(set(section[32:])), 16)

    def test_a_small_picture_keeps_its_size_to_repeat_with_an_even_width(self):
        with tempfile.TemporaryDirectory() as root:
            _, entry = self.entry_of(root, '[wallpaper]\nimage = "paper.png"\n', (33, 20))
            self.assertEqual((32, 20), entry[2:4])

    def test_an_upright_picture_is_fitted_to_the_upright_screen(self):
        with tempfile.TemporaryDirectory() as root:
            _, entry = self.entry_of(root, '[wallpaper_portrait]\nimage = "paper.png"\n', (400, 400))
            self.assertEqual((1, 170, 320), (entry[0], *entry[2:4]))

    def test_dimming_fades_the_picture_into_the_background(self):
        with tempfile.TemporaryDirectory() as root:
            package, entry = self.entry_of(
                root, '[wallpaper]\nimage = "paper.png"\ndim = 90\n', (320, 170))
            palette = struct.unpack_from("<16H", package, entry[5])
            self.assertLess(max(theme_pack.luminance(shade) for shade in palette), 0.01)

    def test_a_missing_or_misnamed_picture_is_an_error(self):
        for wallpaper in ('[wallpaper]\nimage = "none.png"\n',
                          '[wallpaper]\nimage = "../paper.png"\n',
                          '[wallpaper]\nimage = "paper.png"\ndim = 95\n',
                          '[wallpaper]\nimage = "paper.png"\nrows = ["00"]\n',
                          '[wallpaper]\nimage = 7\n'):
            with tempfile.TemporaryDirectory() as root:
                folder, definitions = self.write_theme(root, wallpaper, (16, 16))
                with self.assertRaises(theme_pack.ThemeError):
                    theme_pack.build_all(folder, definitions)


FONTS = [("title", 20), ("body", 20), ("small", 15)]
GLYPH = struct.Struct("<HHBBbbBx")
INTER = TOOLS_DIRECTORY.parent / "assets" / "fonts" / "inter" / "Inter-SemiBold.ttf"
LETTERS = {"ascent": 3, "descent": 1, "glyphs": [
    {"char": "B", "left": 1, "top": 3, "advance": 4, "rows": ["f8", "1.", "ff"]},
    {"char": "A", "left": -1, "top": 2, "advance": 5, "rows": ["fff"]},
    {"char": " ", "left": 0, "top": 0, "advance": 2, "rows": []}]}


def lettered(fonts, base=None):
    return theme_pack.pack({"name": "Mine", "fonts": fonts}, BASE if base is None else base,
                           COLORS, BOXES, fonts=FONTS)


def asset_entries(package):
    colors, boxes, box_size, _, spaces = struct.unpack_from("<HHBBB", package, 16)
    table_at = (56 + 2 * colors + boxes * box_size + spaces + 8 * (package[49] + package[50]) +
                20 * package[51])
    return [struct.unpack_from("<BBHHBBII", package, table_at + 16 * index)
            for index in range(package[48])]


class FontTest(unittest.TestCase):
    def test_a_font_is_an_asset_of_glyph_records_and_levels_in_code_order(self):
        package = lettered({"body": LETTERS})
        kind, ascent, count, bitmap, descent, zero, offset, check = asset_entries(package)[0]
        self.assertEqual((3, 3, 3, 5, 1, 0), (kind, ascent, count, bitmap, descent, zero))
        section = package[offset:]
        self.assertEqual(theme_pack.checksum(section), check)
        self.assertEqual(header(package)[3], offset)
        records = [GLYPH.unpack_from(section, 10 * index) for index in range(3)]
        self.assertEqual([(32, 0, 0, 0, 0, 0, 2), (65, 0, 3, 1, -1, 2, 5), (66, 2, 2, 3, 1, 3, 4)],
                         records)
        self.assertEqual(bytes([0xFF, 0xF0, 0xF8, 0x10, 0xFF]), section[30:])

    def test_each_slot_is_its_own_kind_after_the_wallpapers(self):
        package = lettered({"title": LETTERS, "small": LETTERS})
        self.assertEqual([2, 4], [entry[0] for entry in asset_entries(package)])
        papered = theme_pack.pack({"name": "Mine", "fonts": {"body": LETTERS}, "wallpaper": TILE},
                                  BASE, COLORS, BOXES, fonts=FONTS)
        self.assertEqual([0, 3], [entry[0] for entry in asset_entries(papered)])

    def test_a_font_too_tall_for_its_text_or_misshapen_is_refused(self):
        def changed(**fields):
            return {**LETTERS, **fields}

        for fonts in ({"body": changed(ascent=17, descent=4)}, {"small": changed(ascent=12, descent=4)},
                      {"body": changed(ascent=0)}, {"body": changed(glyphs=[])},
                      {"body": changed(glyphs=LETTERS["glyphs"] * 2)},
                      {"body": changed(glyphs=[{"char": "AB", "left": 0, "top": 0, "advance": 1}])},
                      {"body": changed(glyphs=[{"char": "A", "left": 0, "top": 0, "advance": 256}])},
                      {"body": changed(glyphs=[{"char": "A", "left": 0, "top": 0, "advance": 1,
                                                "rows": ["ff", "f"]}])},
                      {"body": changed(glyphs=[{"char": "A", "left": 0, "top": 0, "advance": 1,
                                                "size": 3}])},
                      {"body": changed(size=16)}, {"heading": LETTERS}, {"body": "Inter"}):
            with self.assertRaises(theme_pack.ThemeError):
                lettered(fonts)

    def test_fonts_and_the_largest_wallpaper_must_fit_the_players_memory(self):
        many = [{"char": chr(32 + index), "left": 0, "top": 0, "advance": 1,
                 "rows": ["f" * 40] * 40} for index in range(9)]
        heavy = {"ascent": 3, "descent": 1, "glyphs": many}
        screen = {"palette": ["#000000"], "rows": ["00" * 160] * 170}
        lettered({"body": heavy})
        theme_pack.pack({"name": "Mine", "wallpaper": screen}, BASE, COLORS, BOXES, fonts=FONTS)
        with self.assertRaisesRegex(theme_pack.ThemeError, "28,224"):
            theme_pack.pack({"name": "Mine", "wallpaper": screen, "fonts": {"body": heavy}},
                            BASE, COLORS, BOXES, fonts=FONTS)

    def test_the_font_list_names_each_slot_and_its_height(self):
        self.assertEqual(FONTS, theme_pack.fonts_in(DEFINITIONS / "theme_fonts.def"))
        self.assertEqual([], theme_pack.fonts_in(DEFINITIONS / "none.def"))

    def test_the_memory_matches_what_the_platform_hands_over(self):
        system = (DEFINITIONS.parents[2] / "hal" / "include" / "hal" / "system.h").read_text(
            encoding="utf-8")
        self.assertIn(f"HAL_THEME_ASSET_BYTES = {theme_pack.ASSET_MEMORY}", system)


@unittest.skipUnless(importlib.util.find_spec("fontTools") and importlib.util.find_spec("PIL"),
                     "fontTools and Pillow are not installed")
class FontFileTest(unittest.TestCase):
    def write_theme(self, root, font):
        folder = Path(root) / "themes"
        folder.mkdir()
        (folder / "Inter-SemiBold.ttf").write_bytes(INTER.read_bytes())
        (folder / "dark.toml").write_text(
            (DEFINITIONS.parents[3] / "assets" / "themes" / "dark.toml").read_text(encoding="utf-8")
            + font, encoding="utf-8")
        return folder

    def body_of(self, root, font):
        package = theme_pack.build_all(self.write_theme(root, font), DEFINITIONS)["dark"]
        return package, asset_entries(package)[0]

    def test_a_font_file_is_drawn_at_the_largest_size_that_fits_its_text(self):
        with tempfile.TemporaryDirectory() as root:
            _, entry = self.body_of(root, '[fonts.body]\nfile = "Inter-SemiBold.ttf"\n')
            self.assertEqual((3, 16, 108, 4), (entry[0], entry[1], entry[2], entry[4]))

    def test_a_font_file_takes_the_size_it_is_given(self):
        with tempfile.TemporaryDirectory() as root:
            _, entry = self.body_of(root, '[fonts.small]\nfile = "Inter-SemiBold.ttf"\nsize = 11\n')
            self.assertEqual((4, 11, 3), (entry[0], entry[1], entry[4]))

    def test_a_font_file_too_big_for_its_text_or_misnamed_is_an_error(self):
        for font in ('[fonts.small]\nfile = "Inter-SemiBold.ttf"\nsize = 16\n',
                     '[fonts.body]\nfile = "none.ttf"\n',
                     '[fonts.body]\nfile = "../Inter-SemiBold.ttf"\n',
                     '[fonts.body]\nfile = "Inter-SemiBold.ttf"\nsize = 40\n',
                     '[fonts.body]\nfile = "Inter-SemiBold.ttf"\nweight = 3\n'):
            with tempfile.TemporaryDirectory() as root:
                with self.assertRaises(theme_pack.ThemeError):
                    self.body_of(root, font)


LAYOUT = [("cover", 112), ("title", 22), ("progress", 3)]
PLACE = struct.Struct("<hhHBB")


def placed(theme, base=None):
    return theme_pack.pack({"name": "Mine", **theme}, BASE if base is None else base, COLORS, BOXES,
                           layout=LAYOUT)


def places(package):
    count = package[49] + package[50]
    return [PLACE.unpack_from(package, 56 + 6 + 24 + PLACE.size * index) for index in range(count)]


ACCENT = struct.Struct("<HHHB13s")
RED = {"name": "Red", "selection_background": "#ff0000", "selection_top": "#ff8000",
       "selection_shine": "#ffffff"}


def accents_of(package):
    count = package[51]
    return [ACCENT.unpack_from(package, 56 + 6 + 24 + ACCENT.size * index) for index in range(count)]


class AccentTest(unittest.TestCase):
    def test_a_theme_without_accents_keeps_the_short_header(self):
        self.assertEqual(48, header(packed({"name": "Mine"}))[2])

    def test_accents_follow_the_places_with_three_shades_and_a_name(self):
        ruby = {**RED, "name": "Ruby"}
        package = packed({"name": "Mine", "accents": [RED, ruby]})
        self.assertEqual(56, header(package)[2])
        self.assertEqual((2, 0), (package[51], package[52]))
        self.assertEqual([(0xF800, 0xFC00, 0xFFFF, 1, bytes(13)),
                          (0xF800, 0xFC00, 0xFFFF, 255, b"Ruby" + bytes(9))], accents_of(package))
        self.assertEqual(len(package), header(package)[3])

    def test_a_standard_colour_name_is_stored_as_its_number_in_any_case(self):
        package = packed({"name": "Mine", "accents": [{**RED, "name": "GRAPHITE"}]})
        self.assertEqual(7, accents_of(package)[0][3])

    def test_accents_are_the_themes_own_and_never_come_from_the_base(self):
        base = {**BASE, "accents": [RED]}
        self.assertEqual(48, header(packed({"name": "Mine"}, base))[2])

    def test_bad_accents_are_refused(self):
        for accents in ([{"name": "Red"}], [{**RED, "name": ""}], [{**RED, "name": "A" * 13}],
                        [{**RED, "glow": "#000000"}], [RED] * 9, RED):
            with self.assertRaises(theme_pack.ThemeError):
                packed({"name": "Mine", "accents": accents})

    def test_cover_flow_can_ask_for_the_dark_look(self):
        package = packed({"name": "Mine", "cover_flow": "dark"})
        self.assertEqual(56, header(package)[2])
        self.assertEqual((0, 1), (package[51], package[52]))
        self.assertEqual(48, header(packed({"name": "Mine", "cover_flow": "theme"}))[2])
        with self.assertRaises(theme_pack.ThemeError):
            packed({"name": "Mine", "cover_flow": "light"})


class LayoutTest(unittest.TestCase):
    def test_a_theme_without_places_keeps_the_short_header(self):
        self.assertEqual(48, header(placed({}))[2])

    def test_places_follow_the_spacing_for_each_way_the_screen_is_held(self):
        package = placed({"now_playing": {"cover": {"x": -4, "y": 30, "size": 64},
                                          "title": {"x": 10, "y": 96, "width": 300,
                                                    "align": "right"}},
                          "now_playing_portrait": {"progress": {"hide": True}}})
        self.assertEqual(56, header(package)[2])
        self.assertEqual((0, 3, 3), (package[48], package[49], package[50]))
        self.assertEqual([(-4, 30, 64, 0, 0), (10, 96, 300, 22, 2), (0, 0, 0, 0, 255),
                          (0, 0, 0, 0, 255), (0, 0, 0, 0, 255), (0, 0, 0, 3, 0)], places(package))
        self.assertEqual(len(package), header(package)[3])

    def test_places_come_from_the_base_unless_the_theme_sets_its_own(self):
        base = {**BASE, "now_playing": {"progress": {"x": 1, "y": 2, "width": 3, "height": 4}}}
        package = placed({"now_playing": {"title": {"x": 5, "y": 6, "width": 7}}}, base)
        self.assertEqual([(0, 0, 0, 0, 255), (5, 6, 7, 22, 0), (1, 2, 3, 4, 0)], places(package))

    def test_a_place_out_of_reach_or_misshapen_is_refused(self):
        for layout in ({"cover": {"x": 0, "y": 0, "size": 171}},
                       {"cover": {"x": 0, "y": 0, "width": 50}},
                       {"title": {"x": 321, "y": 0, "width": 10}},
                       {"title": {"x": 0, "y": -321, "width": 10}},
                       {"title": {"x": 0, "y": 0, "width": 0}},
                       {"title": {"x": 0, "y": 0, "width": 10, "height": 256}},
                       {"title": {"x": 0, "y": 0, "width": 10, "align": "middle"}},
                       {"title": {"x": 0, "y": 0}},
                       {"title": {"hide": True, "x": 0}},
                       {"title": {"x": 0.5, "y": 0, "width": 10}},
                       {"footer": {"x": 0, "y": 0, "width": 10}}, {"title": "top"}):
            with self.assertRaises(theme_pack.ThemeError):
                placed({"now_playing": layout})

    def test_places_with_a_wallpaper_still_leave_its_readability_check_right(self):
        clear = {"boxes": {"header": {"fill": "background", "ink": "text"}}}
        pale = {"palette": ["#000000", "#eeeeee"], "rows": ["0000000011"] * 10}
        package = theme_pack.pack({"name": "Mine", "wallpaper": pale, **clear,
                                   "now_playing": {"title": {"x": 0, "y": 0, "width": 10}}},
                                  BASE, COLORS, BOXES, layout=LAYOUT)
        found = theme_pack.lint(package, COLORS, BOXES)
        self.assertEqual(1, len(found))
        self.assertTrue(found[0].startswith("List text over the wallpaper"))

    def test_the_layout_list_names_each_place_and_its_line_height(self):
        listed = theme_pack.layout_in(DEFINITIONS / "theme_layout.def")
        self.assertEqual(("cover", 112), listed[0])
        self.assertIn(("lyrics", 95), listed)
        self.assertEqual([], theme_pack.layout_in(DEFINITIONS / "none.def"))


ICONS = [("play", 4, 3), ("pause", 4, 3), ("battery", 6, 2), ("battery_level", 6, 2)]
ICONS_AT = 48 + 6 + 24


def packed_with_icons(icons, base=None):
    return theme_pack.pack({"name": "Mine", "icons": icons}, BASE if base is None else base,
                           COLORS, BOXES, icons=ICONS)


class IconTest(unittest.TestCase):
    def test_a_theme_without_icons_is_the_same_file_as_before(self):
        self.assertEqual(packed({"name": "Mine"}), packed_with_icons({}))
        self.assertEqual(0, header(packed_with_icons({}))[10])

    def test_an_icon_is_a_table_entry_and_its_rows_as_nibbles(self):
        package = packed_with_icons({"play": ["f.8", ".1F"]})
        self.assertEqual(1, header(package)[10])
        self.assertEqual(bytes([3, 2, 0, 0, 0xF0, 0x80, 0x01, 0xF0]), package[ICONS_AT:])
        self.assertEqual(len(package), header(package)[3])
        self.assertEqual(theme_pack.checksum(package[16:]), header(package)[4])

    def test_icons_left_out_before_a_given_one_are_empty_entries(self):
        package = packed_with_icons({"pause": ["ff"], "battery": ["1234"],
                                     "battery_level": ["0.00"]})
        self.assertEqual(4, header(package)[10])
        table = struct.unpack_from("<BBHBBHBBHBBH", package, ICONS_AT)
        self.assertEqual((0, 0, 0, 2, 1, 0, 4, 1, 1, 4, 1, 3), table)
        self.assertEqual(bytes([0xFF, 0x12, 0x34, 0x00, 0x00]), package[ICONS_AT + 16:])

    def test_icons_come_from_the_base_unless_the_theme_draws_its_own(self):
        base = {**BASE, "icons": {"play": ["ff"], "pause": ["11"]}}
        package = packed_with_icons({"pause": ["22"]}, base)
        self.assertEqual(bytes([0xFF, 0x22]), package[ICONS_AT + 8:])

    def test_a_battery_and_its_level_go_together_at_one_size(self):
        for icons in ({"battery": ["ff"]}, {"battery_level": ["ff"]},
                      {"battery": ["ff"], "battery_level": ["fff"]}):
            with self.assertRaisesRegex(theme_pack.ThemeError, "go together"):
                packed_with_icons(icons)

    def test_icons_that_are_unknown_misshapen_or_too_big_are_refused(self):
        for icons in ({"stop": ["ff"]}, {"play": []}, {"play": "ff"}, {"play": ["ff", "f"]},
                      {"play": [""]}, {"play": ["fffff"]}, {"play": ["f"] * 4},
                      {"play": ["fg"]}, {"play": [7]}, ["play"]):
            with self.assertRaises(theme_pack.ThemeError):
                packed_with_icons(icons)

    def test_the_icon_list_gives_each_name_and_its_largest_size(self):
        icons = theme_pack.icons_in(DEFINITIONS / "theme_icons.def")
        self.assertEqual(("status_play", 10, 10), icons[0])
        self.assertIn(("battery_level_large", 30, 14), icons)
        names = [name for name, _, _ in icons]
        for pair in theme_pack.ICON_PAIRS:
            self.assertEqual(1, names.index(pair[1]) - names.index(pair[0]))
        self.assertEqual([], theme_pack.icons_in(DEFINITIONS / "none.def"))

    def test_a_theme_with_every_icon_at_its_largest_fits_a_file(self):
        icons = theme_pack.icons_in(DEFINITIONS / "theme_icons.def")
        colors = theme_pack.names_in(DEFINITIONS / "theme_colors.def")
        boxes = theme_pack.names_in(DEFINITIONS / "theme_boxes.def")
        dark = theme_pack.read_source(DEFINITIONS.parents[3] / "assets" / "themes" / "dark.toml")
        full = {name: ["f" * width] * height for name, width, height in icons}
        package = theme_pack.pack({"name": "Full", "icons": full}, dark, colors, boxes, icons=icons)
        self.assertLessEqual(len(package), theme_pack.MAX_SIZE)


CLEAR_HEADER = {"fill": "background", "ink": "text"}
DEFINITIONS = TOOLS_DIRECTORY.parent / "src" / "ui" / "include" / "ui"


def warnings_for(theme):
    clear = {"boxes": {"header": CLEAR_HEADER}, **theme}
    return theme_pack.lint(packed({"name": "Mine", **clear}), COLORS, BOXES)


class LintTest(unittest.TestCase):
    def test_contrast_runs_from_one_to_twenty_one(self):
        self.assertEqual(21.0, theme_pack.contrast(0xFFFF, 0x0000))
        self.assertEqual(1.0, theme_pack.contrast(0x1082, 0x1082))

    def test_a_theme_that_reads_clearly_has_no_warnings(self):
        self.assertEqual([], warnings_for({}))

    def test_text_close_to_its_background_is_warned_with_its_ratio_and_floor(self):
        found = warnings_for({"colors": {"text": "#666666"}})
        self.assertEqual(3, len(found))
        self.assertRegex(found[0], r"^List text: contrast 3\.\d, below 4\.5$")
        self.assertRegex(found[2], r"^The title on the header: contrast 3\.\d, below 4\.5$")

    def test_ink_is_checked_against_the_worse_end_of_a_gradient(self):
        found = warnings_for({"boxes": {"selection": {"fill": ["accent", "text"], "ink": "text"},
                                        "header": CLEAR_HEADER}})
        self.assertEqual(["Text on the selected row: contrast 1.0, below 3.0"], found)

    def test_the_selected_row_check_reads_the_box_the_theme_selects(self):
        murky = {"selection": {"fill": "text", "ink": "text"}, "header": CLEAR_HEADER}
        self.assertEqual(1, len(warnings_for({"boxes": murky})))
        self.assertEqual([], warnings_for({"boxes": murky, "selection": "header"}))

    def test_every_check_names_colours_and_boxes_the_firmware_defines(self):
        colors = theme_pack.names_in(DEFINITIONS / "theme_colors.def")
        boxes = theme_pack.names_in(DEFINITIONS / "theme_boxes.def")
        for check in theme_pack.CHECKS:
            self.assertTrue(theme_pack.applies(check, colors, boxes), check["what"])
        self.assertFalse(theme_pack.applies(theme_pack.CHECKS[1], COLORS, BOXES))


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

    def test_strict_fails_when_a_theme_is_hard_to_read(self):
        with tempfile.TemporaryDirectory() as root:
            sources, definitions = self.write_sources(root)
            arguments = ["--themes", str(sources), "--definitions", str(definitions),
                         "--output", str(Path(root) / "out")]
            self.assertEqual(0, theme_pack.main(arguments))
            self.assertEqual(1, theme_pack.main(arguments + ["--strict"]))
            (sources / "paper.toml").write_text(
                'name = "Paper"\n[colors]\nbackground = "#ffffff"\ntext = "#000000"\n'
                '[boxes.selection]\nfill = "text"\nink = "background"\n', encoding="utf-8")
            (sources / "dark.toml").write_text(
                'name = "Dark"\n[colors]\nbackground = "#000000"\ntext = "#ffffff"\n'
                '[boxes.selection]\nfill = "text"\nink = "background"\n', encoding="utf-8")
            self.assertEqual(0, theme_pack.main(arguments + ["--strict"]))

    def test_a_folder_without_a_dark_theme_is_packed_over_the_base_it_is_given(self):
        with tempfile.TemporaryDirectory() as root:
            sources, definitions = self.write_sources(root)
            shared = Path(root) / "shared"
            shared.mkdir()
            (shared / "mint.toml").write_text('name = "Mint"\n[colors]\ntext = "#00ff00"\n',
                                              encoding="utf-8")
            with self.assertRaises(theme_pack.ThemeError):
                theme_pack.build_all(shared, definitions)
            packages = theme_pack.build_all(shared, definitions, sources / "dark.toml")
            self.assertEqual(["mint"], list(packages))
            self.assertEqual((0x0000, 0x07E0),
                             struct.unpack_from("<2H", packages["mint"], theme_pack.HEADER_SIZE))
            arguments = ["--themes", str(shared), "--definitions", str(definitions),
                         "--base", str(sources / "dark.toml"), "--output", str(Path(root) / "out")]
            self.assertEqual(0, theme_pack.main(arguments))
            self.assertTrue((Path(root) / "out" / "mint.echotheme").is_file())

    def test_a_source_that_is_not_toml_is_reported_by_name(self):
        with tempfile.TemporaryDirectory() as root:
            sources, definitions = self.write_sources(root)
            (sources / "broken.toml").write_text("name = ", encoding="utf-8")
            with self.assertRaisesRegex(theme_pack.ThemeError, "broken.toml"):
                theme_pack.build_all(sources, definitions)


if __name__ == "__main__":
    unittest.main()

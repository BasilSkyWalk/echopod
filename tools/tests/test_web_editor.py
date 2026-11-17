import json
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
REPOSITORY = TOOLS_DIRECTORY.parent
sys.path.insert(0, str(TOOLS_DIRECTORY))

import theme_pack
import web_editor

PAGES = REPOSITORY / "web" / "editor"
THEMES = REPOSITORY / "assets" / "themes"
SHARED = REPOSITORY / "themes"
DEFINITIONS = REPOSITORY / "src" / "ui" / "include" / "ui"
GLYPHS = REPOSITORY / "assets" / "fonts" / "glyph_pack.json"
PACKS = REPOSITORY / "src" / "fonts" / "packs"
BUILT_MODULE = REPOSITORY / "build" / "web" / "platform" / "web" / "echo_preview.wasm"
PNG_MAGIC = b"\x89PNG\r\n\x1a\n"
MURKY = ('name = "Murky"\nauthor = "Someone"\ndescription = "Hard to read on purpose"\n'
         '[colors]\ntext = "#555a60"\ntext_secondary = "#3a3d42"\nselection_text = "#5291f0"\n'
         '[boxes.progress_fill]\nfill = "divider"\n'
         '[wallpaper]\npalette = ["#606060", "#101114"]\nrows = ["0001", "0000"]\n')


def build_site(root, community=None, module=None):
    stand_in = Path(root) / "echo_preview.wasm"
    stand_in.write_bytes(b"\x00asm")
    output = Path(root) / "site"
    arguments = ["--pages", str(PAGES), "--module", str(module or stand_in), "--themes", str(THEMES),
                 "--definitions", str(DEFINITIONS), "--glyphs", str(GLYPHS), "--packs", str(PACKS),
                 "--output", str(output)]
    if community:
        arguments += ["--community", str(community)]
    return web_editor.main(arguments), output


def shared_folder(root, sources):
    folder = Path(root) / "shared"
    folder.mkdir()
    for name, text in sources.items():
        (folder / name).write_text(text, encoding="utf-8")
    return folder


def definition_names():
    return (theme_pack.names_in(DEFINITIONS / "theme_colors.def"),
            theme_pack.names_in(DEFINITIONS / "theme_boxes.def"))


class SchemaTest(unittest.TestCase):
    def test_the_schema_lists_what_a_theme_can_set_in_definition_order(self):
        schema = web_editor.schema(THEMES, DEFINITIONS)
        self.assertEqual("background", schema["colors"][0])
        self.assertEqual("selection", schema["boxes"][0])
        self.assertEqual({"name": "row_height", "least": 18, "most": 40}, schema["spacing"][0])
        self.assertIn("dark", schema["samples"])

    def test_the_dark_theme_is_given_field_by_field(self):
        dark = web_editor.schema(THEMES, DEFINITIONS)["dark"]
        self.assertEqual("selection", dark["selection"])
        self.assertEqual("#101114", dark["colors"]["background"])
        self.assertEqual({"fill_top": "selection_top", "fill_bottom": "selection_background",
                          "border": "background", "shine": "selection_shine",
                          "ink": "selection_text", "border_width": 0, "border_edges": 0,
                          "shine_height": 1, "inset_across": 0, "inset_down": 0, "radius": 0, "see_through": 0},
                         dark["boxes"]["selection"])
        self.assertEqual(2, dark["boxes"]["header"]["border_edges"])
        self.assertEqual([6, 1], [dark["boxes"]["selection_outline"]["inset_across"],
                                  dark["boxes"]["selection_outline"]["inset_down"]])

    def test_the_schema_lists_the_icons_with_their_largest_sizes_and_pairs(self):
        schema = web_editor.schema(THEMES, DEFINITIONS)
        self.assertEqual({"name": "status_play", "width": 10, "height": 10}, schema["icons"][0])
        self.assertEqual(17, len(schema["icons"]))
        self.assertIn(["battery", "battery_level"], schema["icon_pairs"])

    def test_the_schema_lists_the_wallpapers_with_their_screen_sizes(self):
        schema = web_editor.schema(THEMES, DEFINITIONS)
        self.assertEqual([{"name": "wallpaper", "width": 320, "height": 170},
                          {"name": "wallpaper_portrait", "width": 170, "height": 320}],
                         schema["wallpapers"])

    def test_the_schema_lists_the_font_slots_the_characters_and_the_memory(self):
        schema = web_editor.schema(THEMES, DEFINITIONS)
        self.assertEqual([{"name": "title", "height": 20}, {"name": "body", "height": 20},
                          {"name": "small", "height": 15}], schema["fonts"])
        self.assertIn([32, 126], schema["font_ranges"])
        self.assertEqual(28224, schema["asset_memory"])

    def test_the_schema_lists_the_now_playing_places_and_both_layouts(self):
        schema = web_editor.schema(THEMES, DEFINITIONS)
        self.assertEqual({"name": "cover", "height": 112}, schema["layout"][0])
        self.assertEqual(10, len(schema["layout"]))
        self.assertEqual(["now_playing", "now_playing_portrait"], schema["layouts"])

    def test_the_schema_carries_the_packer_readability_checks(self):
        checks = web_editor.schema(THEMES, DEFINITIONS)["checks"]
        self.assertEqual(list(theme_pack.CHECKS), checks)
        self.assertEqual({"pair", "ink", "fills", "wallpaper"},
                         {check["kind"] for check in checks})


class SiteTest(unittest.TestCase):
    def test_the_site_holds_the_pages_the_module_the_card_and_the_packed_themes(self):
        with tempfile.TemporaryDirectory() as root:
            status, output = build_site(root)
            self.assertEqual(0, status)
            for name in (*web_editor.PAGE_FILES, "echo_preview.wasm", "library.db", "covers.bin",
                         "lyrics.lrc", "demo_files.json", "theme_schema.json",
                         "themes/dark.echotheme", "themes/ember.echotheme"):
                self.assertTrue((output / name).is_file(), name)
            schema = json.loads((output / "theme_schema.json").read_text(encoding="utf-8"))
            faces = schema.pop("faces")
            self.assertEqual(web_editor.schema(THEMES, DEFINITIONS), schema)
            self.assertEqual([2, 3, 4], [face["index"] for face in faces])
            for face in faces:
                self.assertTrue((output / face["file"]).is_file(), face["file"])

    def test_every_page_the_site_links_is_one_it_copies(self):
        for name in ("preview_host.js", "icon_editor.js", "wallpaper_editor.js", "font_editor.js",
                     "drawing.js", "drawings.js", "drawing_editor.js", "scene_compile.js", "colour_tools.js", "gallery.html",
                     "gallery.js"):
            self.assertIn(name, web_editor.PAGE_FILES)
        for name in web_editor.PAGE_FILES:
            self.assertTrue((PAGES / name).is_file(), name)

    def test_only_the_small_latin_faces_are_offered_with_their_place_in_the_face_order(self):
        faces = web_editor.glyph_faces(GLYPHS, PACKS)
        self.assertEqual([{"index": 2, "file": "inter_regular_12_latin1.bin"},
                          {"index": 3, "file": "inter_regular_16_latin1.bin"},
                          {"index": 4, "file": "inter_semibold_16_latin1.bin"}], faces)

    def test_a_missing_module_is_an_error(self):
        with tempfile.TemporaryDirectory() as root:
            arguments = ["--pages", str(PAGES), "--module", str(Path(root) / "none.wasm"),
                         "--themes", str(THEMES), "--definitions", str(DEFINITIONS),
                         "--glyphs", str(GLYPHS), "--packs", str(PACKS),
                         "--output", str(Path(root) / "site")]
            self.assertEqual(1, web_editor.main(arguments))


class SharedThemesTest(unittest.TestCase):
    def test_shared_themes_join_the_site_with_their_sources_and_a_gallery_list(self):
        with tempfile.TemporaryDirectory() as root:
            status, output = build_site(root, shared_folder(root, {"murky.toml": MURKY}))
            self.assertEqual(0, status)
            for name in ("murky.echotheme", "murky.toml", "dark.toml", "index.json"):
                self.assertTrue((output / "themes" / name).is_file(), name)
            schema = json.loads((output / "theme_schema.json").read_text(encoding="utf-8"))
            samples = sorted(path.stem for path in THEMES.glob("*.toml"))
            self.assertEqual(samples + ["murky"], schema["samples"])
            listed = json.loads((output / "themes" / "index.json").read_text(encoding="utf-8"))
            self.assertEqual(samples + ["murky"], [entry["file"] for entry in listed])
            self.assertEqual({"file": "murky", "name": "Murky", "author": "Someone",
                              "description": "Hard to read on purpose", "shared": True}, listed[-1])
            self.assertFalse(listed[0]["shared"])

    def test_a_shared_theme_is_packed_over_the_built_in_dark_theme(self):
        with tempfile.TemporaryDirectory() as root:
            _, output = build_site(root, shared_folder(root, {"murky.toml": MURKY}))
            colors, _ = definition_names()
            murky = (output / "themes" / "murky.echotheme").read_bytes()
            dark = (output / "themes" / "dark.echotheme").read_bytes()
            shades = [dict(zip(colors, struct.unpack_from(f"<{len(colors)}H", package,
                                                          struct.unpack_from("<H", package, 6)[0])))
                      for package in (dark, murky)]
            self.assertEqual(shades[0]["background"], shades[1]["background"])
            self.assertNotEqual(shades[0]["text"], shades[1]["text"])

    def test_a_picture_a_shared_theme_names_is_copied_beside_its_source(self):
        with tempfile.TemporaryDirectory() as root:
            status, output = build_site(root, SHARED)
            self.assertEqual(0, status)
            pictures = list(SHARED.glob("*.png"))
            self.assertTrue(pictures)
            for path in pictures:
                self.assertEqual(path.read_bytes(), (output / "themes" / path.name).read_bytes())

    def test_a_shared_theme_may_not_take_a_built_in_file_name_or_an_awkward_one(self):
        for name in ("ember.toml", "My Theme.toml", "Loud.toml", "x" * 24 + ".toml"):
            with tempfile.TemporaryDirectory() as root:
                status, _ = build_site(root, shared_folder(root, {name: MURKY}))
                self.assertEqual(1, status, name)

    def test_the_themes_folder_of_this_repository_packs_and_reads_clearly(self):
        colors, boxes = definition_names()
        taken = {path.stem for path in web_editor.sources_in(THEMES)}
        sources = web_editor.shared_sources(SHARED, taken)
        packages = theme_pack.build_all(SHARED, DEFINITIONS, THEMES / "dark.toml")
        self.assertEqual([path.stem for path in sources], list(packages))
        for path in sources:
            self.assertEqual([], theme_pack.lint(packages[path.stem], colors, boxes), path.name)
            entry = web_editor.gallery_entry(path, True)
            self.assertTrue(entry["author"] and entry["description"], path.name)


@unittest.skipUnless(shutil.which("node"), "node is not installed")
class PageTest(unittest.TestCase):
    def run_node(self, script, *arguments):
        return subprocess.run(["node", str(PAGES / script), *map(str, arguments)],
                              capture_output=True, text=True, check=False)

    def test_the_page_packs_unpacks_and_checks_themes_exactly_as_the_packer_does(self):
        with tempfile.TemporaryDirectory() as root:
            _, output = build_site(root, shared_folder(root, {"murky.toml": MURKY}))
            colors, boxes = definition_names()
            expected = {path.stem: theme_pack.lint(path.read_bytes(), colors, boxes)
                        for path in (output / "themes").glob("*.echotheme")}
            self.assertGreaterEqual(len(expected["murky"]), 5)
            self.assertIn("List text over the wallpaper: contrast 1.2, below 3.0",
                          expected["murky"])
            self.assertEqual([], expected["dark"])
            warnings = Path(root) / "warnings.json"
            warnings.write_text(json.dumps(expected), encoding="utf-8")
            result = self.run_node("test_theme_format.mjs", output, warnings)
            self.assertEqual(0, result.returncode, result.stderr)

    @unittest.skipUnless(BUILT_MODULE.is_file(), "the preview module is not built")
    def test_every_theme_of_a_site_is_drawn_to_a_picture(self):
        with tempfile.TemporaryDirectory() as root:
            _, output = build_site(root, SHARED, BUILT_MODULE)
            result = self.run_node("render_theme.mjs", output)
            self.assertEqual(0, result.returncode, result.stderr)
            for stem in ("dark", "ember", *(path.stem for path in SHARED.glob("*.toml"))):
                picture = (output / "previews" / (stem + ".png")).read_bytes()
                self.assertEqual(PNG_MAGIC, picture[:8], stem)
                self.assertEqual((992, 692), (int.from_bytes(picture[16:20], "big"),
                                              int.from_bytes(picture[20:24], "big")), stem)

    @unittest.skipUnless(BUILT_MODULE.is_file(), "the preview module is not built")
    def test_the_preview_plays_pauses_moves_and_changes_song_as_asked(self):
        with tempfile.TemporaryDirectory() as root:
            _, output = build_site(root, None, BUILT_MODULE)
            result = self.run_node("test_preview_states.mjs", output)
            self.assertEqual(0, result.returncode, result.stderr)

    def test_the_drawing_tools_snap_group_line_up_and_copy_between_screens(self):
        result = self.run_node("test_drawing_editor.mjs")
        self.assertEqual(0, result.returncode, result.stderr)

    def test_drawing_needs_a_site_folder(self):
        self.assertEqual(2, self.run_node("render_theme.mjs").returncode)


if __name__ == "__main__":
    unittest.main()

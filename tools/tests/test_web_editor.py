import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
REPOSITORY = TOOLS_DIRECTORY.parent
sys.path.insert(0, str(TOOLS_DIRECTORY))

import web_editor

PAGES = REPOSITORY / "web" / "editor"
THEMES = REPOSITORY / "assets" / "themes"
DEFINITIONS = REPOSITORY / "src" / "ui" / "include" / "ui"
GLYPHS = REPOSITORY / "assets" / "fonts" / "glyph_pack.json"
PACKS = REPOSITORY / "src" / "fonts" / "packs"


def build_site(root):
    module = Path(root) / "echo_preview.wasm"
    module.write_bytes(b"\x00asm")
    output = Path(root) / "site"
    arguments = ["--pages", str(PAGES), "--module", str(module), "--themes", str(THEMES),
                 "--definitions", str(DEFINITIONS), "--glyphs", str(GLYPHS), "--packs", str(PACKS),
                 "--output", str(output)]
    return web_editor.main(arguments), output


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
                          "shine_height": 1, "inset_across": 0, "inset_down": 0, "radius": 0},
                         dark["boxes"]["selection"])
        self.assertEqual(2, dark["boxes"]["header"]["border_edges"])
        self.assertEqual([6, 1], [dark["boxes"]["selection_outline"]["inset_across"],
                                  dark["boxes"]["selection_outline"]["inset_down"]])


class SiteTest(unittest.TestCase):
    def test_the_site_holds_the_pages_the_module_the_card_and_the_packed_themes(self):
        with tempfile.TemporaryDirectory() as root:
            status, output = build_site(root)
            self.assertEqual(0, status)
            for name in (*web_editor.PAGE_FILES, "echo_preview.wasm", "library.db", "covers.bin",
                         "theme_schema.json", "themes/dark.echotheme", "themes/ember.echotheme"):
                self.assertTrue((output / name).is_file(), name)
            schema = json.loads((output / "theme_schema.json").read_text(encoding="utf-8"))
            faces = schema.pop("faces")
            self.assertEqual(web_editor.schema(THEMES, DEFINITIONS), schema)
            self.assertEqual([2, 3, 4], [face["index"] for face in faces])
            for face in faces:
                self.assertTrue((output / face["file"]).is_file(), face["file"])

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

    @unittest.skipUnless(shutil.which("node"), "node is not installed")
    def test_the_page_packs_and_unpacks_themes_exactly_as_the_packer_does(self):
        with tempfile.TemporaryDirectory() as root:
            _, output = build_site(root)
            result = subprocess.run(["node", str(PAGES / "test_theme_format.mjs"), str(output)],
                                    capture_output=True, text=True, check=False)
            self.assertEqual(0, result.returncode, result.stderr)


if __name__ == "__main__":
    unittest.main()

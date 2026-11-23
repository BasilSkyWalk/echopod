import struct
import sys
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIRECTORY))

import scene_compile

ROOT = TOOLS_DIRECTORY.parent
NAMES = scene_compile.definitions(ROOT / "src" / "np_faces" / "include" / "np_faces")
OPS = NAMES["ops"]
CODES = {entry["code"]: name for name, entry in OPS.items()}


def compiled(source):
    return scene_compile.compile_scene(source, NAMES)


def decoded(code):
    found, at = [], 0
    while at < len(code):
        name = CODES[code[at]]
        count = OPS[name]["count"]
        operands = list(struct.unpack_from(f"<{count}H", code, at + 1))
        found.append((name, operands))
        at += 1 + 2 * count
    return found


def immediate(value):
    return 0x8000 | (value & 0x7FFF)


class LanguageTest(unittest.TestCase):
    def test_a_scene_ends_with_end(self):
        code, strings = compiled("")
        self.assertEqual([("end", [])], decoded(code))
        self.assertEqual(b"", strings)

    def test_constant_arguments_are_folded_into_immediates(self):
        code, _ = compiled("rect(1 + 2, 10 * 3, -4, 7 / 2, 5 % 3)")
        self.assertEqual(("rect", [immediate(3), immediate(30), immediate(-4), immediate(3),
                                   immediate(2)]), decoded(code)[0])

    def test_folding_follows_c_integer_rules(self):
        cases = {"-7 / 2": -3, "-7 % 2": -1, "7 / 0": 0, "1 << 31": -(1 << 31),
                 "-1 >>> 28": 15, "-16 >> 2": -4, "65536 * 65536": 0, "3 > 2": 1,
                 "2 >= 3": 0, "1 && 0": 0, "0 || 5": 1, "!0": 1, "min(3, -2)": -2,
                 "abs(-9)": 9, "1 ? 4 : 5": 4}
        for text, value in cases.items():
            compiler = scene_compile.Compiler(NAMES)
            node = scene_compile.Parser(text).expression()
            self.assertEqual(value, compiler.constant(node), text)

    def test_a_colour_becomes_rgb565_loaded_whole(self):
        code, _ = compiled("rect(0, 0, 1, 1, #ffffff)")
        steps = decoded(code)
        self.assertEqual("const", steps[0][0])
        self.assertEqual([0xFFFF, 0], steps[0][1][1:])

    def test_inputs_are_the_first_registers(self):
        code, _ = compiled("rect(x, y, width, height, tick)")
        self.assertEqual(("rect", [0, 1, 2, 3, NAMES["inputs"].index("tick")]), decoded(code)[0])

    def test_text_names_sources_and_literals_share_a_string_table(self):
        code, strings = compiled('text("A", title, 0, 0, 9, 9, left, 1)\n'
                                 'text("A", body, 0, 0, 9, 9, center, 1)\n'
                                 'text(song, small, 0, 0, 9, 9, right, 1)')
        steps = decoded(code)
        self.assertEqual(immediate(64), steps[0][1][0])
        self.assertEqual(immediate(64), steps[1][1][0])
        self.assertEqual(immediate(NAMES["sources"].index("song")), steps[2][1][0])
        self.assertEqual(b"A\0", strings)

    def test_an_if_jumps_over_what_it_skips(self):
        code, _ = compiled("if playing { rect(0, 0, 1, 1, 2) } else { rect(1, 1, 1, 1, 2) }")
        steps = decoded(code)
        self.assertEqual("jump_zero", steps[0][0])
        self.assertEqual(["jump_zero", "rect", "jump", "rect", "end"], [name for name, _ in steps])

    def test_a_known_condition_keeps_only_its_branch(self):
        code, _ = compiled("const WIDE = 1\nif WIDE { rect(0, 0, 1, 1, 2) } else { outline(0, 0, 1, 1, 2) }")
        self.assertEqual(["rect", "end"], [name for name, _ in decoded(code)])

    def test_a_repeat_counts_up_to_its_end(self):
        code, _ = compiled("repeat i from 0 to 3 { rect(i, 0, 1, 1, 2) }")
        names = [name for name, _ in decoded(code)]
        self.assertEqual(["set", "set", "lt", "jump_zero", "rect", "add", "jump", "end"], names)

    def test_names_made_in_a_block_end_with_it(self):
        compiled("if playing { let a = 1 rect(a, 0, 1, 1, 2) } else { let a = 2 rect(a, 0, 1, 1, 2) }")
        with self.assertRaises(scene_compile.SceneError):
            compiled("if playing { let a = 1 } rect(a, 0, 1, 1, 2)")

    def test_mistakes_are_reported(self):
        for source in ("rect(1, 2)", "let x = 1", "tick = 3", "foo(1)", "rect(0, 0, 1, 1, sin(1, 2))",
                       "let a = 1\nlet a = 2", "if 1 { rect(0, 0, 1, 1, 2)", "const C = tick",
                       "rect(0, 0, 1, 1, @)", "sin(1)"):
            with self.assertRaises(scene_compile.SceneError, msg=source):
                compiled(source)

    def test_home_reads_the_menu_through_functions(self):
        code, _ = compiled("let k = selected()\nlet n = items()\nline(item(k), body, 0, 0, 10, 10, left, 0)\n"
                           "menu(0, 20, 100, 60)\nbackdrop(x, y, width, height)\nreflect(0, 0, 50, 20)")
        names = [name for name, _ in decoded(code)]
        self.assertEqual(["selected", "items", "item", "line", "menu", "backdrop", "reflect", "end"], names)

    def test_a_tile_draws_its_items_icon(self):
        code, _ = compiled("repeat n from 0 to items() {\n    item_icon(n, n * 40, 0, 24, 24, #ffffff)\n}")
        self.assertIn("item_icon", [name for name, _ in decoded(code)])

    def test_new_operations_leave_the_older_codes_alone(self):
        self.assertEqual(56, OPS["outline"]["code"])
        self.assertTrue(all(OPS[name]["code"] > OPS["outline"]["code"]
                            for name in ("item", "items", "selected", "menu", "reflect", "backdrop")))
        self.assertEqual(OPS["backdrop"]["code"] + 1, OPS["item_icon"]["code"])

    def test_every_built_in_face_compiles(self):
        for path in sorted((ROOT / "assets" / "faces").glob("*.scene")):
            code, _ = compiled(path.read_text(encoding="utf-8"))
            self.assertLess(len(code), 6144, path.name)


if __name__ == "__main__":
    unittest.main()

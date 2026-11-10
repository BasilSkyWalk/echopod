import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIRECTORY))

import check_np_faces_sections as check

GOOD = """#include "core/resident.h"
#define SIZE(a) \\
    (sizeof(a) / \\
     sizeof((a)[0]))
typedef struct Layout { int x; } Layout;
enum { COUNT = 3 };
extern const int OTHER;
ECHO_NP_FACES_DATA static const Layout LAYOUT = {1};
ECHO_NP_FACES_DATA static const char NAME[] = "a;{b";
ECHO_NP_FACES_ENTRY const Layout ENTRY = {2};
ECHO_NP_FACES_CODE static int twice(int value)
{
    int local = value * 2;
    return local;
}
ECHO_NP_FACES_CODE void draw(const Layout *layout, int (*hook)(int));
static int counter;
"""


class SectionMacroTest(unittest.TestCase):
    def test_a_fully_marked_file_is_clean(self):
        self.assertEqual([], check.violations(GOOD))

    def test_an_unmarked_table_is_found_with_its_line(self):
        text = GOOD.replace("ECHO_NP_FACES_DATA static const Layout LAYOUT",
                            "static const Layout LAYOUT")
        found = check.violations(text)
        self.assertEqual(1, len(found))
        self.assertEqual("static const Layout LAYOUT = {1};", text.splitlines()[found[0][0] - 1])
        self.assertIn("ECHO_NP_FACES_DATA", found[0][1])

    def test_an_unmarked_function_is_found(self):
        text = GOOD.replace("ECHO_NP_FACES_CODE static int twice", "static int twice")
        found = check.violations(text)
        self.assertEqual(1, len(found))
        self.assertIn("ECHO_NP_FACES_CODE", found[0][1])

    def test_an_unmarked_scalar_constant_is_found(self):
        self.assertEqual(1, len(check.violations("static const int LIMIT = 4;\n")))

    def test_a_static_const_inside_a_function_is_found(self):
        text = GOOD.replace("int local = value * 2;",
                            "static const int steps[2] = {1, 2};\n    int local = value * 2;")
        found = check.violations(text)
        self.assertEqual(1, len(found))
        self.assertIn("inside a function", found[0][1])

    def test_main_fails_on_an_unmarked_table_and_passes_on_a_clean_tree(self):
        with tempfile.TemporaryDirectory() as directory:
            faces = Path(directory) / "src" / "np_faces"
            faces.mkdir(parents=True)
            (faces / "face.c").write_text(GOOD)
            self.assertEqual(0, check.main([directory]))
            (faces / "bad.c").write_text("static const int TABLE[2] = {1, 2};\n")
            self.assertEqual(1, check.main([directory]))


if __name__ == "__main__":
    unittest.main()

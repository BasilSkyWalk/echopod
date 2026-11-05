import sys
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIRECTORY))

import size_report

MAP = """Archive member included to satisfy reference by file (symbol)

src/app/libecho_app.a(app.c.obj)
                              platform/device/CMakeFiles/echo_device.dir/board/main.c.obj (app_start)

Linker script and memory map

LOAD platform/device/CMakeFiles/echo_device.dir/board/audio.c.obj

.code           0x03050000     0x1000
 *(.code_vectors)
 .code_vectors  0x03050000       0x40 platform/device/CMakeFiles/echo_device.dir/board/startup.c.obj
 *(.text .text.*)
 .text.pump     0x03050040       0x8c platform/device/CMakeFiles/echo_device.dir/board/audio.c.obj
 .text.hal_audio_play
                0x030500cc       0x40 platform/device/CMakeFiles/echo_device.dir/board/audio.c.obj
                0x030500cc                hal_audio_play
 *fill*         0x0305010c        0x4
 .text.app_step
                0x03050110      0x200 C:\\Users\\x\\Temp\\ccfpNjjI.ltrans0.ltrans.o
 .text.list_view_draw
                0x03050310      0x100 src/ui/libecho_ui.a(list_view.c.obj)
 *(.rodata .rodata.*)
 .rodata.INTER_REGULAR_12_BITMAP
                0x03050410      0x300 src/fonts/libecho_fonts.a(fonts.c.obj)
 .text          0x03050710      0x2ba C:/msys64/clang64/lib/gcc/arm-none-eabi/16.2.0/thumb/v7-m/nofp\\libgcc.a(_udivmoddi4.o)

.data           0x03004000        0x8 load address 0x03051000
 *(.entry_vector)
 .entry_vector  0x03004000        0x8 platform/device/CMakeFiles/echo_device.dir/board/startup.c.obj

.bss            0x03004008    0x20000 load address 0x03051008
 *(.bss .bss.*)
 .bss.music_ring
                0x03004008    0x1b580 platform/device/CMakeFiles/echo_device.dir/board/audio.c.obj
 .bss.app       0x0301f588     0x4a78 C:\\Users\\x\\Temp\\ccfpNjjI.ltrans0.ltrans.o

.noinit         0x03024008       0x30
 .noinit.record 0x03024008       0x30 platform/device/CMakeFiles/echo_device.dir/board/start_info.c.obj

.high_ram       0x0307a000    0x12050
 .high_ram.decoding
                0x0307a000     0xa050 platform/device/CMakeFiles/echo_device.dir/board/audio.c.obj
"""

OVERLAY_MAP = """Linker script and memory map

.overlay        0x0307a000    0x14000
 .text.scan     0x0307a010     0x1000 src/library/libecho_library_scan.a(library_scan.c.obj)
"""

NM = """03050110 00000200 T app_step\tD:/Projects/echopod/src/app/app.c:200
0301f588 00004a78 b app\tD:/Projects/echopod/src/app/app.c:133
030500cc 00000040 T hal_audio_play\tD:/Projects/echopod/platform/device/board/audio.c:50
03050710 000002ba t __udivmoddi4
"""


class ParseMapTest(unittest.TestCase):
    def test_lists_every_placed_input_section_with_its_output(self):
        pieces = size_report.parse_map(MAP)
        by_name = {piece.section: piece for piece in pieces}
        self.assertEqual(by_name[".text.pump"].output, ".code")
        self.assertEqual(by_name[".text.pump"].size, 0x8C)
        self.assertEqual(by_name[".text.hal_audio_play"].address, 0x030500CC)
        self.assertEqual(by_name[".text.app_step"].size, 0x200)
        self.assertEqual(by_name[".bss.music_ring"].output, ".bss")
        self.assertEqual(by_name[".high_ram.decoding"].output, ".high_ram")
        fills = [piece for piece in pieces if piece.section == size_report.FILL]
        self.assertEqual([piece.size for piece in fills], [4])

    def test_output_sizes_come_from_the_section_headers(self):
        sizes = size_report.output_sizes(MAP)
        self.assertEqual(sizes[".code"], 0x1000)
        self.assertEqual(sizes[".data"], 8)
        self.assertEqual(sizes[".bss"], 0x20000)


class NamingTest(unittest.TestCase):
    def test_archive_members_become_library_and_source(self):
        self.assertEqual(size_report.object_name("src/app/libecho_app.a(app.c.obj)"), "app/app.c")
        self.assertEqual(size_report.object_name("src/library/libecho_library.a(library.c.obj)"),
                         "library/library.c")
        self.assertEqual(
            size_report.object_name("platform/device/logic/libecho_device_logic.a(msc.c.obj)"),
            "device_logic/msc.c")

    def test_plain_objects_keep_their_source_path(self):
        self.assertEqual(
            size_report.object_name(
                "platform/device/CMakeFiles/echo_device.dir/board/audio.c.obj"),
            "board/audio.c")

    def test_libgcc_members_are_grouped_under_gcc(self):
        origin = "C:/msys64/clang64/lib/gcc/arm-none-eabi/16.2.0/thumb/v7-m/nofp\\libgcc.a(_udivmoddi4.o)"
        self.assertEqual(size_report.object_name(origin), "gcc/_udivmoddi4")

    def test_lto_pieces_are_attributed_through_nm(self):
        nm_files = size_report.parse_nm(NM)
        pieces = {piece.section: piece for piece in size_report.parse_map(MAP)}
        self.assertEqual(size_report.attribute(pieces[".text.app_step"], nm_files), "app/app.c")
        self.assertEqual(size_report.attribute(pieces[".bss.app"], nm_files), "app/app.c")
        self.assertEqual(size_report.attribute(pieces[".text.pump"], nm_files), "board/audio.c")

    def test_lto_pieces_without_line_information_are_marked(self):
        pieces = {piece.section: piece for piece in size_report.parse_map(MAP)}
        self.assertEqual(size_report.attribute(pieces[".text.app_step"], {}), size_report.UNKNOWN)

    def test_symbol_names_drop_the_section_prefix(self):
        pieces = {piece.section: piece for piece in size_report.parse_map(MAP)}
        self.assertEqual(size_report.symbol_name(pieces[".text.pump"]), "pump")
        self.assertEqual(size_report.symbol_name(pieces[".rodata.INTER_REGULAR_12_BITMAP"]),
                         "INTER_REGULAR_12_BITMAP")


class ReportTest(unittest.TestCase):
    def test_summary_measures_against_each_limit(self):
        lines = size_report.summary(MAP, OVERLAY_MAP)
        self.assertEqual(lines[0], "code window: 4,096 of 157,292 bytes, 153,196 free")
        self.assertEqual(lines[1], "loaded total (code + .data + .bss): 135,176 of 437,600 bytes, "
                                   "302,424 free")
        self.assertEqual(lines[2], "rescan overlay: 81,920 of 86,016 bytes, 4,096 free")

    def test_summary_flags_a_limit_that_is_over(self):
        over = MAP.replace(".code           0x03050000     0x1000",
                           ".code           0x03050000    0x26700")
        self.assertIn("OVER BY 148", size_report.summary(over)[0])

    def test_report_groups_code_by_object_and_lists_the_biggest_symbols(self):
        text = size_report.report(MAP, OVERLAY_MAP, NM, top=3, objects=10)
        self.assertIn("code window: 4,096 of 157,292 bytes, 153,196 free", text)
        self.assertIn("code window: .text 1,670, .rodata 768, fill 4, vectors and the rest 1,654",
                      text)
        lines = text.splitlines()
        objects = lines[lines.index("largest objects (of 7):") + 2:]
        self.assertTrue(objects[0].startswith("fonts/fonts.c"), objects[0])
        self.assertIn("768", objects[0])
        self.assertTrue(objects[1].startswith("gcc/_udivmoddi4"), objects[1])
        app = next(line for line in objects if line.startswith("app/app.c"))
        self.assertIn("512", app)
        self.assertIn("19,064", app)
        self.assertIn("INTER_REGULAR_12_BITMAP", text)
        self.assertIn("music_ring", text)
        self.assertIn("112,000", text)
        self.assertNotIn(size_report.FILL, text.split("largest objects")[1])


if __name__ == "__main__":
    unittest.main()

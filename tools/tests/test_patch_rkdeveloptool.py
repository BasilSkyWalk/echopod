import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIRECTORY))

import patch_rkdeveloptool

COMM_HEADER = "#pragma pack()\n#define CMD_TIMEOUT 0\n#define CBW_SIGN\t\t\t0x43425355\n"
MAIN = "#include <stdio.h>\n#define DEFAULT_RW_LBA 128\nint main(void);\n"
SCAN = (
    "\tconfig.emDeviceType = RKNANO_DEVICE;\n"
    "\tconfig.usPid = 0x3226;\n"
    "\tconfig.usVid = 0x071B;\n"
    "\tm_deviceConfigSet.push_back(config);\n"
    "\n"
    "\t\t\tdesc.pUsbHandle = (void *)dev;\n"
    "\t\t\tdesc.usbcdUsb = descriptor.bcdUSB;\n"
)


def make_tree(directory):
    root = Path(directory)
    (root / "RKComm.h").write_text(COMM_HEADER, newline="\n")
    (root / "main.cpp").write_text(MAIN, newline="\n")
    (root / "RKScan.cpp").write_text(SCAN, newline="\n")
    return root


class PatchTest(unittest.TestCase):
    def test_the_command_timeout_becomes_5000_ms(self):
        with tempfile.TemporaryDirectory() as directory:
            root = make_tree(directory)
            patch_rkdeveloptool.patch(root)
            self.assertIn("#define CMD_TIMEOUT 5000\n", (root / "RKComm.h").read_text())

    def test_transfers_are_32_sectors(self):
        with tempfile.TemporaryDirectory() as directory:
            root = make_tree(directory)
            patch_rkdeveloptool.patch(root)
            self.assertIn("#define DEFAULT_RW_LBA 32\n", (root / "main.cpp").read_text())

    def test_the_echo_mini_maskrom_id_is_a_known_rknano_device(self):
        with tempfile.TemporaryDirectory() as directory:
            root = make_tree(directory)
            patch_rkdeveloptool.patch(root)
            scan = (root / "RKScan.cpp").read_text()
            self.assertIn("config.usPid = 0x262D;\n\tconfig.usVid = 0x2207;", scan)

    def test_the_echo_mini_is_classified_by_bcd_device(self):
        with tempfile.TemporaryDirectory() as directory:
            root = make_tree(directory)
            patch_rkdeveloptool.patch(root)
            scan = (root / "RKScan.cpp").read_text()
            self.assertIn("descriptor.idProduct == 0x262D", scan)
            self.assertIn("? descriptor.bcdDevice : descriptor.bcdUSB", scan)

    def test_patching_twice_is_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            root = make_tree(directory)
            patch_rkdeveloptool.patch(root)
            with self.assertRaises(patch_rkdeveloptool.PatchError):
                patch_rkdeveloptool.patch(root)

    def test_an_unexpected_source_is_refused_without_changing_anything(self):
        with tempfile.TemporaryDirectory() as directory:
            root = make_tree(directory)
            (root / "main.cpp").write_text("#define DEFAULT_RW_LBA 64\n", newline="\n")
            with self.assertRaises(patch_rkdeveloptool.PatchError):
                patch_rkdeveloptool.patch(root)
            self.assertEqual(COMM_HEADER, (root / "RKComm.h").read_text())


if __name__ == "__main__":
    unittest.main()

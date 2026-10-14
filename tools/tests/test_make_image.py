import functools
import os
import struct
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIRECTORY))

import make_image

REFERENCE_PATH = Path(os.environ.get(
    "ECHO_REFERENCE_IMG",
    Path.home() / "echo-mini-blobs/recho-0.9.2/HIFIEC90.IMG"))

RESET = 0x03050101
IMAGE_SIZE = 0x500004
FW_SIZE = 0x44C000
CODE_OFFSET = 0xD68


def word(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def row(data, index):
    return struct.unpack_from("<4I", data, 0x208 + 16 * index)


@functools.cache
def fake_reference():
    image = bytearray(IMAGE_SIZE)
    image[:0x1F4] = bytes((index * 7 + 3) & 0xFF for index in range(0x1F4))
    struct.pack_into("<I", image, 0x1F4, FW_SIZE)
    image[0x1F8:0x200] = b"RKnanoFW"
    image[FW_SIZE:FW_SIZE + 0x200] = image[:0x200]
    image[FW_SIZE + 0x200:FW_SIZE + 0x208] = b"\x11" * 8
    struct.pack_into("<I", image, 0x500000, make_image.crc32(bytes(image[:0x500000])))
    return bytes(image)


def vector_table(stack=0x03004000, reset=RESET, size=256):
    return struct.pack("<II", stack, reset) + bytes(size - 8)


def firmware(code_tail=b"\xAA" * 64, data_tail=b"", bss_size=0x100, stack=0x03004000,
             reset=RESET, code_reset=RESET):
    return make_image.Firmware(
        code=vector_table(reset=code_reset) + code_tail,
        data=struct.pack("<II", stack, reset) + data_tail,
        bss_size=bss_size)


def build(firmware_parts=None, **options):
    settings = {"brand": "ECHOFW", "build_tag": "abc1234", "date": (2026, 10, 14)}
    settings.update(options)
    return make_image.build_image(fake_reference(), firmware_parts or firmware(), **settings)


class CrcTest(unittest.TestCase):
    def test_the_empty_crc_is_zero(self):
        self.assertEqual(0, make_image.crc32(b""))

    def test_appending_the_crc_big_endian_leaves_a_zero_residue(self):
        data = b"Echo Mini"
        self.assertEqual(0, make_image.crc32(data + make_image.crc32(data).to_bytes(4, "big")))

    def test_skipping_zero_runs_matches_the_byte_by_byte_crc(self):
        for zeros in (255, 256, 257, 4096, 100003):
            data = b"\x12\x34" + bytes(zeros) + b"\x56" + bytes(zeros)
            self.assertEqual(make_image.crc_bytes(0, data), make_image.crc32(data), zeros)

    def test_the_polynomial_is_not_the_standard_one(self):
        self.assertEqual(0x04C10DB7, make_image.crc32(b"\x01"))


class FwEndTest(unittest.TestCase):
    def test_the_reference_fw_size_ends_at_5_mib(self):
        self.assertEqual(0x500000, make_image.fw_end(0x44C000))

    def test_the_stock_fw_size_ends_at_32_mib(self):
        self.assertEqual(0x2000000, make_image.fw_end(0x01FC4000))


class LayoutTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.parts = firmware(data_tail=b"\x55" * 8)
        cls.image = build(cls.parts)
        cls.code_size = len(cls.parts.code)
        cls.data_size = len(cls.parts.data)

    def test_the_image_is_5_mib_plus_the_trailer(self):
        self.assertEqual(IMAGE_SIZE, len(self.image))

    def test_the_outer_header_keeps_the_reference_bytes_outside_the_patched_fields(self):
        reference = fake_reference()
        patched = set(range(0x00, 0x04)) | set(range(0x10, 0x18)) | set(range(0x30, 0x38))
        patched |= set(range(0x80, 0x84))
        for offset in range(0x1F4):
            if offset not in patched:
                self.assertEqual(reference[offset], self.image[offset], hex(offset))

    def test_the_fw_size_word_is_the_header_copy_offset(self):
        self.assertEqual(FW_SIZE, word(self.image, 0x1F4))

    def test_the_container_header_has_the_magic_base_and_capacity(self):
        self.assertEqual(b"RKnanoFW", self.image[0x1F8:0x200])
        self.assertEqual(0x03050000, word(self.image, 0x200))
        self.assertEqual(91, word(self.image, 0x204))

    def test_row_0_loads_the_code_block(self):
        self.assertEqual((0x03050B68, 0x03050000, self.code_size, 0x03050B68 + self.code_size),
                         row(self.image, 0))

    def test_row_1_places_data_and_records_the_data_and_bss_span(self):
        self.assertEqual((0x03004000, self.data_size, 0x03004000 + self.data_size,
                          self.data_size + 0x100), row(self.image, 1))

    def test_row_2_is_the_constant_from_the_reference(self):
        self.assertEqual((0x03078CD0, 0x03079DCC, 4, 0), row(self.image, 2))

    def test_the_remaining_rows_and_the_prefix_are_zero(self):
        self.assertFalse(any(self.image[0x208 + 3 * 16:CODE_OFFSET]))

    def test_the_code_starts_at_0xd68_and_the_data_follows_it(self):
        code_end = CODE_OFFSET + self.code_size
        self.assertEqual(self.parts.code, self.image[CODE_OFFSET:code_end])
        self.assertEqual(self.parts.data, self.image[code_end:code_end + self.data_size])

    def test_everything_between_the_payload_and_the_header_copy_is_zero(self):
        end = CODE_OFFSET + self.code_size + self.data_size
        self.assertFalse(any(self.image[end:FW_SIZE]))

    def test_the_header_copy_repeats_the_first_512_bytes(self):
        self.assertEqual(self.image[:0x200], self.image[FW_SIZE:FW_SIZE + 0x200])

    def test_the_marker_bytes_come_from_the_reference(self):
        self.assertEqual(b"\x11" * 8, self.image[FW_SIZE + 0x200:FW_SIZE + 0x208])
        self.assertFalse(any(self.image[FW_SIZE + 0x208:0x500000]))

    def test_the_trailer_is_the_crc_of_everything_before_it(self):
        self.assertEqual(make_image.crc32(self.image[:0x500000]), word(self.image, 0x500000))

    def test_the_built_image_passes_the_updater_checks(self):
        self.assertEqual([], make_image.validate_image(self.image))


class HeaderFieldTest(unittest.TestCase):
    def test_the_date_is_bcd_month_day_year(self):
        self.assertEqual(0x10142026, word(build(), 0x00))

    def test_the_brand_and_build_tag_are_nul_padded_ascii(self):
        image = build(brand="ECHOFW", build_tag="abc1234")
        self.assertEqual(b"ECHOFW\x00\x00", image[0x10:0x18])
        self.assertEqual(b"abc1234\x00", image[0x30:0x38])

    def test_field_0x80_is_the_end_of_the_data_image(self):
        parts = firmware()
        image = build(parts)
        self.assertEqual(CODE_OFFSET + len(parts.code) + len(parts.data), word(image, 0x80))

    def test_field_0x80_leaves_out_trailing_zero_words_of_the_data(self):
        parts = firmware(data_tail=bytes(8))
        image = build(parts)
        self.assertEqual(CODE_OFFSET + len(parts.code) + 8, word(image, 0x80))

    def test_field_0x80_rounds_the_last_non_zero_byte_up_to_a_word(self):
        parts = firmware(data_tail=b"\x01\x00\x00\x00")
        image = build(parts)
        self.assertEqual(CODE_OFFSET + len(parts.code) + 12, word(image, 0x80))

    def test_a_brand_longer_than_8_bytes_is_refused(self):
        with self.assertRaises(make_image.ImageError):
            build(brand="TOOLONGBRAND")

    def test_a_non_ascii_build_tag_is_refused(self):
        with self.assertRaises(make_image.ImageError):
            build(build_tag="é")


class FirmwareRulesTest(unittest.TestCase):
    def assert_refused(self, parts):
        with self.assertRaises(make_image.ImageError):
            build(parts)

    def test_data_shorter_than_the_entry_vector_is_refused(self):
        parts = firmware()
        self.assert_refused(make_image.Firmware(code=parts.code, data=b"\x00" * 4, bss_size=0))

    def test_an_entry_stack_other_than_0x03004000_is_refused(self):
        self.assert_refused(firmware(stack=0x03003000))

    def test_a_reset_handler_without_the_thumb_bit_is_refused(self):
        self.assert_refused(firmware(reset=0x03050100, code_reset=0x03050100))

    def test_a_reset_handler_outside_the_code_block_is_refused(self):
        self.assert_refused(firmware(reset=0x03004001, code_reset=0x03004001))

    def test_a_code_vector_that_disagrees_with_the_entry_vector_is_refused(self):
        self.assert_refused(firmware(code_reset=0x03050201))

    def test_data_larger_than_the_proven_180_bytes_is_refused(self):
        self.assert_refused(firmware(data_tail=b"\x00" * 176))

    def test_data_of_exactly_180_bytes_is_accepted(self):
        self.assertEqual(IMAGE_SIZE, len(build(firmware(data_tail=b"\x00" * 172))))

    def test_code_past_the_reference_row_0_end_is_refused(self):
        limit = 0x030771D4 - 0x03050B68 - 256
        self.assert_refused(firmware(code_tail=b"\x00" * (limit + 4)))

    def test_code_that_ends_at_the_reference_row_0_end_is_accepted(self):
        limit = 0x030771D4 - 0x03050B68 - 256
        self.assertEqual(IMAGE_SIZE, len(build(firmware(code_tail=b"\x00" * limit))))

    def test_data_and_bss_beyond_0x0304c000_are_refused(self):
        self.assert_refused(firmware(bss_size=0x48000))

    def test_unaligned_code_is_refused(self):
        self.assert_refused(firmware(code_tail=b"\x00" * 3))

    def test_unaligned_data_is_refused(self):
        self.assert_refused(firmware(data_tail=b"\x00" * 2))


class ValidatorTest(unittest.TestCase):
    def test_a_missing_magic_is_reported(self):
        image = bytearray(build())
        image[0x1F8:0x200] = b"XXXXXXXX"
        self.assertIn("magic", " ".join(make_image.validate_image(bytes(image))))

    def test_a_missing_header_copy_is_reported(self):
        image = bytearray(build())
        image[FW_SIZE:FW_SIZE + 0x200] = bytes(0x200)
        self.assertIn("header copy", " ".join(make_image.validate_image(bytes(image))))

    def test_a_bad_crc_is_reported(self):
        image = bytearray(build())
        image[0x500000] ^= 1
        self.assertIn("crc", " ".join(make_image.validate_image(bytes(image))))

    def test_a_short_file_is_reported(self):
        self.assertTrue(make_image.validate_image(build()[:0x400000]))


class ReferenceCheckTest(unittest.TestCase):
    def test_a_file_that_is_not_the_reference_is_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "HIFIEC90.IMG"
            path.write_bytes(fake_reference())
            with self.assertRaises(make_image.ImageError):
                make_image.load_reference(path)


@unittest.skipUnless(REFERENCE_PATH.exists(), f"reference image not found at {REFERENCE_PATH}")
class RealReferenceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.reference = make_image.load_reference(REFERENCE_PATH)

    def test_the_reference_trailer_matches_the_facts_sheet_and_passes_the_updater_checks(self):
        self.assertEqual(0xEFDC758B, word(self.reference, 0x500000))
        self.assertEqual([], make_image.validate_image(self.reference))

    def test_reassembling_the_reference_from_its_parts_is_byte_identical(self):
        parts = make_image.split_image(self.reference)
        self.assertEqual(self.reference, make_image.assemble(parts))

    def test_an_image_built_on_the_reference_passes_the_updater_checks(self):
        image = make_image.build_image(self.reference, firmware(), brand="ECHOFW",
                                       build_tag="abc1234", date=(2026, 10, 14))
        self.assertEqual([], make_image.validate_image(image))

    def test_the_command_writes_a_valid_image(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            parts = firmware()
            (root / "code.bin").write_bytes(parts.code)
            (root / "data.bin").write_bytes(parts.data)
            output = root / "HIFIEC91.IMG"
            status = make_image.main([
                "--reference", str(REFERENCE_PATH),
                "--code", str(root / "code.bin"), "--data", str(root / "data.bin"),
                "--bss-size", "256", "--build-tag", "abc1234", "--date", "2026-10-14",
                "--output", str(output)])
            self.assertEqual(0, status)
            self.assertEqual([], make_image.validate_image(output.read_bytes()))


if __name__ == "__main__":
    unittest.main()

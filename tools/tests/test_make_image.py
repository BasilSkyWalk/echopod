import datetime
import re
import subprocess
import struct
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIRECTORY))

import make_image

RESET = 0x03050101
IMAGE_SIZE = 0x500004
FW_SIZE = 0x44C000
CODE_OFFSET = 0xD68


def word(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def row(data, index):
    return struct.unpack_from("<4I", data, 0x208 + 16 * index)


def vector_table(stack=0x03004000, reset=RESET, size=256):
    return struct.pack("<II", stack, reset) + bytes(size - 8)


def firmware(code_tail=b"\xAA" * 64, data_tail=b"", bss_size=0x100, stack=0x03004000,
             reset=RESET, code_reset=RESET, resident=b"", np_faces=b""):
    return make_image.Firmware(
        code=vector_table(reset=code_reset) + code_tail,
        data=struct.pack("<II", stack, reset) + data_tail,
        bss_size=bss_size, resident=resident, np_faces=np_faces)


def mock_nm(output):
    result = subprocess.CompletedProcess([], 0, stdout=output, stderr="")
    return mock.patch.object(make_image.subprocess, "run", return_value=result)


def build(firmware_parts=None, **options):
    settings = {"brand": "ECHOFW", "build_tag": "abc1234", "date": (2026, 10, 14)}
    settings.update(options)
    return make_image.build_image(firmware_parts or firmware(), **settings)


SHF_ALLOC = 2
SHT_PROGBITS = 1
SHT_STRTAB = 3
SHT_NOBITS = 8


def elf_file(sections):
    sections = [("", 0, 0, 0, b"", 0), *sections]
    names = b"\x00"
    name_offsets = []
    for section in sections[1:]:
        name_offsets.append(len(names))
        names += section[0].encode() + b"\x00"
    name_offsets.append(len(names))
    names += b".shstrtab\x00"
    sections.append((".shstrtab", SHT_STRTAB, 0, 0, names, len(names)))
    body = bytearray(52)
    file_offsets = []
    for _, kind, _, _, content, _ in sections:
        file_offsets.append(len(body))
        body += b"" if kind == SHT_NOBITS else content
        body += bytes(-len(body) % 4)
    headers_offset = len(body)
    for index, (_, kind, flags, address, _, size) in enumerate(sections):
        name = 0 if index == 0 else name_offsets[index - 1]
        body += struct.pack("<10I", name, kind, flags, address, file_offsets[index], size,
                            0, 0, 4, 0)
    body[:52] = b"\x7fELF\x01\x01\x01" + bytes(9) + struct.pack(
        "<HHIIIIIHHHHHH", 2, 40, 1, RESET, 0, headers_offset, 0, 52, 0, 0, 40,
        len(sections), len(sections) - 1)
    return bytes(body)


def firmware_elf(parts, extra=()):
    sections = [
        (".code", SHT_PROGBITS, SHF_ALLOC, 0x03050000, parts.code, len(parts.code)),
        (".data", SHT_PROGBITS, SHF_ALLOC | 1, 0x03004000, parts.data, len(parts.data)),
        (".bss", SHT_NOBITS, SHF_ALLOC | 1, 0x03004000 + len(parts.data), b"", parts.bss_size),
        *extra,
    ]
    return elf_file(sections)


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


class BrandTest(unittest.TestCase):
    def test_the_default_brand_is_echopod(self):
        arguments = make_image.parse_arguments(["--elf", "a.elf", "--output", "b.img"])
        self.assertEqual("EchoPod", arguments.brand)


class FwEndTest(unittest.TestCase):
    def test_our_fw_size_ends_at_5_mib(self):
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

    def test_row_2_is_the_fixed_constant(self):
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

    def test_everything_after_the_header_copy_is_zero(self):
        self.assertFalse(any(self.image[FW_SIZE + 0x200:0x500000]))

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

    def test_code_past_the_row_0_end_is_refused(self):
        limit = 0x030771D4 - 0x03050B68 - 256
        self.assert_refused(firmware(code_tail=b"\x00" * (limit + 4)))

    def test_code_that_ends_at_the_row_0_end_is_accepted(self):
        limit = 0x030771D4 - 0x03050B68 - 256
        self.assertEqual(IMAGE_SIZE, len(build(firmware(code_tail=b"\x00" * limit))))

    def test_data_and_bss_beyond_0x0304c000_are_refused(self):
        self.assert_refused(firmware(bss_size=0x48000))

    def test_code_data_and_bss_past_the_proven_total_are_refused(self):
        self.assert_refused(firmware(code_tail=b"\x00" * 150000, bss_size=288000))

    def test_code_data_and_bss_at_the_proven_total_are_accepted(self):
        code = 150000
        bss = make_image.LOADED_LIMIT - 256 - code - 8
        self.assertEqual(IMAGE_SIZE, len(build(firmware(code_tail=b"\x00" * code, bss_size=bss))))

    def test_unaligned_code_is_refused(self):
        self.assert_refused(firmware(code_tail=b"\x00" * 3))

    def test_unaligned_data_is_refused(self):
        self.assert_refused(firmware(data_tail=b"\x00" * 2))


class ElfTest(unittest.TestCase):
    def test_the_code_data_and_bss_sections_become_the_firmware_parts(self):
        parts = firmware(data_tail=b"\x01\x02\x03\x04", bss_size=0x400)
        self.assertEqual(parts, make_image.firmware_from_elf(firmware_elf(parts)))

    def test_a_missing_bss_section_means_no_bss(self):
        parts = firmware(bss_size=0)
        elf = elf_file([
            (".code", SHT_PROGBITS, SHF_ALLOC, 0x03050000, parts.code, len(parts.code)),
            (".data", SHT_PROGBITS, SHF_ALLOC | 1, 0x03004000, parts.data, len(parts.data)),
        ])
        self.assertEqual(parts, make_image.firmware_from_elf(elf))

    def test_code_linked_at_another_address_is_refused(self):
        parts = firmware()
        elf = elf_file([
            (".code", SHT_PROGBITS, SHF_ALLOC, 0x03060000, parts.code, len(parts.code)),
            (".data", SHT_PROGBITS, SHF_ALLOC | 1, 0x03004000, parts.data, len(parts.data)),
        ])
        with self.assertRaises(make_image.ImageError):
            make_image.firmware_from_elf(elf)

    def test_bss_that_does_not_follow_the_data_is_refused(self):
        parts = firmware()
        elf = elf_file([
            (".code", SHT_PROGBITS, SHF_ALLOC, 0x03050000, parts.code, len(parts.code)),
            (".data", SHT_PROGBITS, SHF_ALLOC | 1, 0x03004000, parts.data, len(parts.data)),
            (".bss", SHT_NOBITS, SHF_ALLOC | 1, 0x03008000, b"", 16),
        ])
        with self.assertRaises(make_image.ImageError):
            make_image.firmware_from_elf(elf)

    def test_any_other_loaded_section_is_refused(self):
        parts = firmware()
        extra = [(".stray", SHT_PROGBITS, SHF_ALLOC, 0x03070000, b"\x00" * 4, 4)]
        with self.assertRaises(make_image.ImageError):
            make_image.firmware_from_elf(firmware_elf(parts, extra))

    def test_sections_that_are_not_loaded_are_ignored(self):
        parts = firmware()
        extra = [(".comment", SHT_PROGBITS, 0, 0, b"GCC\x00", 4)]
        self.assertEqual(parts, make_image.firmware_from_elf(firmware_elf(parts, extra)))

    def test_a_file_that_is_not_a_32_bit_little_endian_elf_is_refused(self):
        with self.assertRaises(make_image.ImageError):
            make_image.firmware_from_elf(b"\x7fELF\x02\x01" + bytes(60))


class DefaultsTest(unittest.TestCase):
    def test_the_default_build_tag_is_the_short_commit_with_a_plus_when_dirty(self):
        tag = make_image.default_build_tag(TOOLS_DIRECTORY.parent)
        self.assertRegex(tag, r"^[0-9a-f]{7}\+?$")

    def test_a_directory_outside_git_gets_an_unknown_tag(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual("unknown", make_image.default_build_tag(Path(directory)))

    def test_the_default_date_is_today(self):
        today = datetime.date.today()
        self.assertEqual((today.year, today.month, today.day), make_image.default_date())


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


class GeneratedHeaderTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.image = make_image.build_image(firmware(), brand="ECHOFW", build_tag="abc1234",
                                           date=(2026, 10, 22), version=(0, 1, 2))

    def test_the_header_holds_the_format_constants(self):
        self.assertEqual(0x76543210, word(self.image, 0x00C))
        self.assertEqual(4, word(self.image, 0x050))
        for offset in (0x054, 0x05C, 0x064, 0x06C):
            self.assertEqual(0x200, word(self.image, offset), hex(offset))
        self.assertEqual(1, word(self.image, 0x19C))
        self.assertEqual(FW_SIZE, word(self.image, 0x1F4))

    def test_the_version_is_major_and_minor_in_0x004_and_the_patch_in_0x008(self):
        self.assertEqual(0x00010000, word(self.image, 0x004))
        self.assertEqual(2, word(self.image, 0x008))

    def test_every_other_header_byte_is_zero(self):
        written = set(range(0x000, 0x010)) | set(range(0x010, 0x018)) | set(range(0x030, 0x038))
        written |= set(range(0x050, 0x070)) | set(range(0x080, 0x084))
        written |= set(range(0x19C, 0x1A0)) | set(range(0x1F4, 0x1F8))
        for offset in range(0x1F4):
            if offset not in written:
                self.assertEqual(0, self.image[offset], hex(offset))

    def test_the_part_chain_is_empty(self):
        for offset in (0x070, 0x074, 0x078, 0x07C, 0x084, 0x088, 0x08C):
            self.assertEqual(0, word(self.image, offset), hex(offset))

    def test_the_date_brand_tag_and_fw1_end_are_ours(self):
        self.assertEqual(0x10222026, word(self.image, 0x000))
        self.assertEqual(b"ECHOFW\0\0", self.image[0x10:0x18])
        self.assertEqual(b"abc1234\0", self.image[0x30:0x38])
        self.assertEqual(CODE_OFFSET + 320 + 8, word(self.image, 0x080))

    def test_the_marker_bytes_are_zero(self):
        self.assertFalse(any(self.image[FW_SIZE + 0x200:FW_SIZE + 0x208]))

    def test_the_header_copy_matches_and_the_image_passes_the_updater_checks(self):
        self.assertEqual(self.image[:0x200], self.image[FW_SIZE:FW_SIZE + 0x200])
        self.assertEqual([], make_image.validate_image(self.image))

    def test_a_version_part_out_of_range_is_refused(self):
        for version in ((256, 0, 0), (0, 256, 0), (0, 0, 0x10000), (-1, 0, 0)):
            with self.assertRaises(make_image.ImageError, msg=str(version)):
                make_image.build_image(firmware(), brand="ECHOFW", build_tag="abc1234",
                                       date=(2026, 10, 22), version=version)

    def test_the_command_builds_an_image(self):
        self.assertNotIn("reference", vars(make_image.parse_arguments(["--elf", "a", "--output", "b"])))

    def test_the_command_writes_a_valid_image(self):
        with tempfile.TemporaryDirectory() as directory:
            elf = Path(directory) / "firmware.elf"
            elf.write_bytes(firmware_elf(firmware()))
            output = Path(directory) / "HIFIEC90.IMG"
            status = make_image.main(["--elf", str(elf), "--output", str(output),
                                      "--build-tag", "abc1234", "--date", "2026-10-22",
                                      "--version", "0.1.2"])
            self.assertEqual(0, status)
            image = output.read_bytes()
            self.assertEqual([], make_image.validate_image(image))
            self.assertEqual(0x00010000, word(image, 0x004))
            self.assertEqual(0, word(image, 0x070))


class SectionCheckTest(unittest.TestCase):
    def sections_with(self, name, kind):
        code = make_image.Section(1, make_image.SHF_ALLOC, 0x03050000, b"\0" * 4, 4)
        data = make_image.Section(1, make_image.SHF_ALLOC, 0x03004000, b"\0" * 8, 8)
        content = b"" if kind == make_image.SHT_NOBITS else b"\0" * 32
        extra = make_image.Section(kind, make_image.SHF_ALLOC, 0x03044000, content, 32)
        return {".code": code, ".data": data, name: extra}

    def test_an_uninitialised_section_outside_the_image_is_allowed(self):
        make_image.check_sections(self.sections_with(".noinit", make_image.SHT_NOBITS))

    def test_a_loaded_section_outside_the_image_is_refused(self):
        with self.assertRaises(make_image.ImageError):
            make_image.check_sections(self.sections_with(".stray", 1))


if __name__ == "__main__":
    unittest.main()


class OverlayTest(unittest.TestCase):
    def overlay(self, body=b"\x5A" * 100):
        return bytes(16) + body

    def test_the_overlay_lands_at_its_offset_with_size_and_crc(self):
        overlay = self.overlay()
        image = build(overlay=overlay)
        offset = make_image.OVERLAY_OFFSET
        self.assertEqual(b"EOVL", image[offset:offset + 4])
        self.assertEqual(len(overlay), word(image, offset + 4))
        self.assertEqual(make_image.crc32(overlay[16:]), word(image, offset + 8))
        self.assertEqual(overlay[16:], image[offset + 16:offset + len(overlay)])
        self.assertEqual([], make_image.validate_image(image))

    def test_the_overlay_header_names_the_code_it_was_linked_against(self):
        parts = firmware(code_tail=b"\x11\x22\x33\x44" * 16)
        image = build(parts, overlay=self.overlay())
        offset = make_image.OVERLAY_OFFSET
        self.assertEqual(make_image.crc32(parts.code), word(image, offset + 12))
        self.assertEqual(parts.code, image[CODE_OFFSET:CODE_OFFSET + len(parts.code)])
        self.assertNotEqual(word(image, offset + 12), word(build(overlay=self.overlay()), offset + 12))

    def test_the_fw1_end_ignores_the_overlay(self):
        self.assertEqual(word(build(), make_image.FW1_END_OFFSET),
                         word(build(overlay=self.overlay()), make_image.FW1_END_OFFSET))

    def test_an_overlay_bigger_than_high_ram_is_refused(self):
        with self.assertRaises(make_image.ImageError):
            build(overlay=self.overlay(bytes(make_image.OVERLAY_CAPACITY)))

    def test_the_overlay_comes_from_its_elf_section(self):
        content = self.overlay(b"\x01\x02\x03\x04")
        elf = elf_file([(".overlay", SHT_PROGBITS, SHF_ALLOC, 0x0307A000, content, len(content))])
        self.assertEqual(content, make_image.overlay_from_elf(elf))

    def test_an_overlay_elf_with_writable_data_is_refused(self):
        content = self.overlay()
        elf = elf_file([(".overlay", SHT_PROGBITS, SHF_ALLOC, 0x0307A000, content, len(content)),
                        (".data", SHT_PROGBITS, SHF_ALLOC | 1, 0x0307B000, b"\x01", 1)])
        with self.assertRaises(make_image.ImageError):
            make_image.overlay_from_elf(elf)

    def test_the_usb_overlay_lands_at_its_own_offset_with_its_own_magic(self):
        parts = firmware(code_tail=b"Uf" * 32)
        overlay = self.overlay(b"w" * 300)
        image = build(parts, overlay=self.overlay(), usb_overlay=overlay)
        offset = make_image.USB_OVERLAY_OFFSET
        self.assertEqual(b"EUSB", image[offset:offset + 4])
        self.assertEqual(len(overlay), word(image, offset + 4))
        self.assertEqual(make_image.crc32(overlay[16:]), word(image, offset + 8))
        self.assertEqual(make_image.crc32(parts.code), word(image, offset + 12))
        self.assertEqual(overlay[16:], image[offset + 16:offset + len(overlay)])
        self.assertEqual(b"EOVL", image[make_image.OVERLAY_OFFSET:make_image.OVERLAY_OFFSET + 4])
        self.assertEqual([], make_image.validate_image(image))

    def test_the_usb_overlay_sits_after_the_glyph_space_inside_the_header_copy(self):
        end = make_image.USB_OVERLAY_OFFSET + make_image.USB_OVERLAY_CAPACITY
        self.assertEqual(make_image.FW_SIZE, end)
        self.assertEqual(0, make_image.USB_OVERLAY_OFFSET % 512)
        self.assertIn((make_image.GLYPH_TABLE_OFFSET + make_image.GLYPH_TABLE_SIZE,
                       make_image.USB_OVERLAY_OFFSET), make_image.GLYPH_REGIONS)

    def test_a_usb_overlay_bigger_than_its_space_is_refused(self):
        with self.assertRaises(make_image.ImageError):
            build(usb_overlay=self.overlay(bytes(make_image.USB_OVERLAY_CAPACITY)))

    def test_a_usb_overlay_elf_may_have_zeroed_memory_after_its_code(self):
        content = self.overlay(b"")
        bss = (".bss", SHT_NOBITS, SHF_ALLOC | 1, 0x0307A000 + len(content), b"", 0x8000)
        elf = elf_file([(".overlay", SHT_PROGBITS, SHF_ALLOC, 0x0307A000, content, len(content)),
                        bss])
        self.assertEqual(content, make_image.overlay_from_elf(elf, zeroed=True))
        with self.assertRaises(make_image.ImageError):
            make_image.overlay_from_elf(elf)

    def test_zeroed_memory_past_the_window_is_refused(self):
        content = self.overlay()
        size = make_image.OVERLAY_CAPACITY - len(content) + 4
        elf = elf_file([(".overlay", SHT_PROGBITS, SHF_ALLOC, 0x0307A000, content, len(content)),
                        (".bss", SHT_NOBITS, SHF_ALLOC | 1, 0x0307A000 + len(content), b"",
                         size)])
        with self.assertRaises(make_image.ImageError):
            make_image.overlay_from_elf(elf, zeroed=True)

    def test_zeroed_memory_over_the_code_is_refused(self):
        content = self.overlay()
        elf = elf_file([(".overlay", SHT_PROGBITS, SHF_ALLOC, 0x0307A000, content, len(content)),
                        (".bss", SHT_NOBITS, SHF_ALLOC | 1, 0x0307A000, b"", 64)])
        with self.assertRaises(make_image.ImageError):
            make_image.overlay_from_elf(elf, zeroed=True)


def symbol_elf(symbols):
    names = bytearray(1)
    table = bytes(16)
    for name, binding, kind, index in symbols:
        table += struct.pack("<IIIBBH", len(names), 0x0307A000, 4, binding << 4 | kind, 0, index)
        names += name.encode() + bytes(1)
    return elf_file([(".symtab", 2, 0, 0, table, len(table)),
                     (".strtab", SHT_STRTAB, 0, 0, bytes(names), len(names))])


class OverlayCopiesTest(unittest.TestCase):
    MAIN = [("hash_update", 0, 2, 1), ("i18n_text.isra.0", 0, 2, 1), ("app_step", 1, 2, 1)]

    def test_an_overlay_copy_of_a_main_function_is_found_even_when_lto_renamed_it(self):
        overlay = [("hash_update", 1, 2, 1), ("i18n_text", 1, 2, 1), ("msc_init", 1, 2, 1)]
        copies = make_image.overlay_copies(symbol_elf(self.MAIN), symbol_elf(overlay))
        self.assertEqual(["hash_update", "i18n_text"], copies)

    def test_symbols_taken_from_the_main_program_and_local_names_are_not_copies(self):
        overlay = [("app_step", 1, 2, 0xFFF1), ("hash_update", 0, 2, 1), ("app_step", 1, 2, 0)]
        self.assertEqual([], make_image.overlay_copies(symbol_elf(self.MAIN), symbol_elf(overlay)))

    def test_a_copy_stops_the_image(self):
        overlay = symbol_elf([("hash_update", 1, 2, 1)])
        with self.assertRaises(make_image.ImageError):
            make_image.check_overlay_copies(symbol_elf(self.MAIN), overlay, "rescan overlay")


class ResidentTablesTest(unittest.TestCase):
    def resident(self, body=b"\x3C" * 100):
        return bytes(16) + body

    def test_the_tables_land_at_their_offset_with_size_and_crc(self):
        resident = self.resident()
        image = build(firmware(resident=resident))
        offset = make_image.RESIDENT_OFFSET
        self.assertEqual(b"ERES", image[offset:offset + 4])
        self.assertEqual(len(resident), word(image, offset + 4))
        self.assertEqual(make_image.crc32(resident[16:]), word(image, offset + 8))
        self.assertEqual(resident[16:], image[offset + 16:offset + len(resident)])
        self.assertEqual([], make_image.validate_image(image))

    def test_the_tables_header_names_the_code_they_belong_to(self):
        parts = firmware(code_tail=b"!C" * 40, resident=self.resident())
        image = build(parts)
        self.assertEqual(make_image.crc32(parts.code), word(image, make_image.RESIDENT_OFFSET + 12))

    def test_the_faces_sit_at_the_top_of_the_free_space_before_the_overlay(self):
        opus_end = (make_image.CORE1_OPUS_OFFSET + make_image.CORE1_MODULE_HEADER_SIZE
                    + make_image.CORE1_MODULE_CAPACITY)
        self.assertEqual(opus_end, make_image.RESIDENT_OFFSET)
        self.assertEqual(make_image.OVERLAY_OFFSET,
                         make_image.NP_FACES_OFFSET + make_image.NP_FACES_CAPACITY)
        self.assertEqual(make_image.GLYPH_REGIONS[0][1], make_image.NP_FACES_OFFSET)
        self.assertEqual(0x0308F000, make_image.NP_FACES_END)
        self.assertEqual(3, len(make_image.GLYPH_REGIONS))

    def test_the_tables_are_not_loaded_at_start(self):
        with_tables = build(firmware(resident=self.resident()))
        without = build()
        self.assertEqual(row(without, 0), row(with_tables, 0))
        self.assertEqual(row(without, 1), row(with_tables, 1))
        self.assertEqual(word(without, make_image.FW1_END_OFFSET),
                         word(with_tables, make_image.FW1_END_OFFSET))

    def test_tables_bigger_than_their_ram_are_refused(self):
        with self.assertRaises(make_image.ImageError):
            build(firmware(resident=self.resident(bytes(make_image.RESIDENT_CAPACITY))))

    def test_tables_without_a_header_or_in_odd_bytes_are_refused(self):
        with self.assertRaises(make_image.ImageError):
            build(firmware(resident=b"\x01" * 8))
        with self.assertRaises(make_image.ImageError):
            build(firmware(resident=self.resident(b"\x01" * 7)))

    def test_the_tables_come_from_the_elf_resident_section(self):
        parts = firmware(resident=self.resident(b"\x01\x02\x03\x04"))
        section = (".resident", SHT_PROGBITS, SHF_ALLOC, 0x0309A800, parts.resident,
                   len(parts.resident))
        self.assertEqual(parts, make_image.firmware_from_elf(firmware_elf(parts, [section])))

    def test_a_resident_section_elsewhere_is_refused(self):
        parts = firmware(resident=self.resident())
        section = (".resident", SHT_PROGBITS, SHF_ALLOC, 0x03092000, parts.resident,
                   len(parts.resident))
        with self.assertRaises(make_image.ImageError):
            make_image.firmware_from_elf(firmware_elf(parts, [section]))


def encode_branch(address, target, link=True):
    offset = target - (address + 4)
    sign = offset >> 24 & 1
    first = 0xF000 | sign << 10 | offset >> 12 & 0x3FF
    second = ((0xD000 if link else 0x9000) | (1 ^ (offset >> 23 & 1) ^ sign) << 13
              | (1 ^ (offset >> 22 & 1) ^ sign) << 11 | offset >> 1 & 0x7FF)
    return struct.pack("<HH", first, second)


def decoded(patch, address):
    return make_image.thumb_branch_target(address, *struct.unpack("<HH", patch))


class NpFacesTest(unittest.TestCase):
    def faces(self, body=b"\x5A" * 100):
        return bytes(16) + body

    def test_the_faces_land_at_their_offset_with_size_and_both_crcs(self):
        faces = self.faces()
        parts = firmware(code_tail=b"!C" * 40, np_faces=faces)
        image = build(parts)
        offset = make_image.NP_FACES_OFFSET
        self.assertEqual(0x1FC000, offset)
        self.assertEqual(b"ENPF", image[offset:offset + 4])
        self.assertEqual(len(faces), word(image, offset + 4))
        self.assertEqual(make_image.crc32(faces[16:]), word(image, offset + 8))
        self.assertEqual(make_image.crc32(parts.code), word(image, offset + 12))
        self.assertEqual(faces[16:], image[offset + 16:offset + len(faces)])
        self.assertEqual([], make_image.validate_image(image))

    def test_the_faces_are_not_loaded_at_start(self):
        with_faces = build(firmware(np_faces=self.faces()))
        without = build()
        self.assertEqual(row(without, 0), row(with_faces, 0))
        self.assertEqual(row(without, 1), row(with_faces, 1))
        self.assertEqual(word(without, make_image.FW1_END_OFFSET),
                         word(with_faces, make_image.FW1_END_OFFSET))

    def test_the_faces_leave_the_resident_tables_alone(self):
        tables = bytes(16) + b"\x3C" * 100
        image = build(firmware(resident=tables, np_faces=self.faces()))
        offset = make_image.RESIDENT_OFFSET
        self.assertEqual(tables[16:], image[offset + 16:offset + len(tables)])

    def test_faces_bigger_than_their_ram_are_refused(self):
        with self.assertRaises(make_image.ImageError):
            build(firmware(np_faces=self.faces(bytes(make_image.NP_FACES_CAPACITY))))

    def test_faces_without_a_header_or_in_odd_bytes_are_refused(self):
        with self.assertRaises(make_image.ImageError):
            build(firmware(np_faces=b"\x01" * 8))
        with self.assertRaises(make_image.ImageError):
            build(firmware(np_faces=self.faces(b"\x01" * 7)))

    def test_the_faces_come_from_the_elf_section_at_their_address(self):
        parts = firmware(np_faces=self.faces(b"\x01\x02\x03\x04"))
        section = (".np_faces", SHT_PROGBITS, SHF_ALLOC, 0x0308B000, parts.np_faces,
                   len(parts.np_faces))
        self.assertEqual(parts, make_image.firmware_from_elf(firmware_elf(parts, [section])))

    def test_a_faces_section_elsewhere_is_refused(self):
        parts = firmware(np_faces=self.faces())
        section = (".np_faces", SHT_PROGBITS, SHF_ALLOC, 0x0308D000, parts.np_faces,
                   len(parts.np_faces))
        with self.assertRaises(make_image.ImageError):
            make_image.firmware_from_elf(firmware_elf(parts, [section]))

    def test_the_new_field_is_last_in_the_image_parts(self):
        self.assertEqual("np_faces", list(make_image.ImageParts.__dataclass_fields__)[-1])


class WindowReferenceTest(unittest.TestCase):
    INSIDE = 0x0308B010
    HERE = make_image.CODE_BASE + 256

    def code_with(self, patch):
        code = bytearray(vector_table() + b"\xAA" * 64)
        code[256:256 + len(patch)] = patch
        return bytes(code)

    def build_code(self, code, sanctioned=(), regions=()):
        data = firmware().data
        return build(make_image.Firmware(code=code, data=data, bss_size=0x100,
                                         sanctioned=sanctioned, regions=regions))

    def test_the_encoder_and_decoder_agree(self):
        for target in (0x03050000, 0x03051000, 0x0308B010, 0x0308EFFC):
            self.assertEqual(target, decoded(encode_branch(0x03050100, target), 0x03050100))
            self.assertEqual(target, decoded(encode_branch(0x03050100, target, False), 0x03050100))

    def test_a_bl_into_the_faces_region_stops_the_build(self):
        with self.assertRaises(make_image.ImageError) as raised:
            self.build_code(self.code_with(encode_branch(self.HERE, self.INSIDE)))
        self.assertIn("0x03050100", str(raised.exception))

    def test_a_b_w_into_the_faces_region_stops_the_build(self):
        with self.assertRaises(make_image.ImageError):
            self.build_code(self.code_with(encode_branch(self.HERE, self.INSIDE + 0x100, False)))

    def test_a_conditional_branch_into_the_faces_region_is_decoded(self):
        offset = 0x0308BA20 - (self.HERE + 4)
        first = 0xF000 | (offset >> 20 & 1) << 10 | 1 << 6 | offset >> 12 & 0x3F
        second = 0x8000 | (offset >> 18 & 1) << 13 | (offset >> 19 & 1) << 11 | offset >> 1 & 0x7FF
        self.assertEqual(0x0308BA20, make_image.thumb_branch_target(self.HERE, first, second))
        code = self.code_with(struct.pack("<HH", first, second))
        with self.assertRaises(make_image.ImageError):
            self.build_code(code)

    def test_the_edges_of_the_region_are_inside_and_outside(self):
        for target in (0x0308B000, 0x0308EFFC):
            with self.assertRaises(make_image.ImageError):
                self.build_code(self.code_with(encode_branch(self.HERE, target)))
        for target in (0x0308F000, 0x0308AFFC):
            self.build_code(self.code_with(encode_branch(self.HERE, target)))

    def test_a_bl_to_the_main_program_is_fine(self):
        self.build_code(self.code_with(encode_branch(self.HERE, make_image.CODE_BASE + 0x2000)))

    def pool(self, *values):
        code = self.code_with(b"".join(struct.pack("<I", value) for value in values))
        return code, ((make_image.CODE_BASE, "t"), (self.HERE, "d"))

    def test_a_pool_word_in_the_region_stops_the_build(self):
        code, regions = self.pool(self.INSIDE)
        with self.assertRaises(make_image.ImageError) as raised:
            self.build_code(code, regions=regions)
        self.assertIn("0x03050100", str(raised.exception))

    def test_a_pool_word_just_outside_the_region_is_fine(self):
        for value in (make_image.NP_FACES_END, make_image.NP_FACES_BASE - 4):
            code, regions = self.pool(value)
            self.build_code(code, regions=regions)

    def test_a_sanctioned_pool_word_passes_and_the_one_next_to_it_does_not(self):
        code, regions = self.pool(self.INSIDE, self.INSIDE)
        self.build_code(code, ((self.HERE, self.HERE + 8),), regions)
        with self.assertRaises(make_image.ImageError):
            self.build_code(code, ((self.HERE, self.HERE + 4),), regions)

    def test_a_word_in_a_code_run_is_not_a_pool_word(self):
        self.build_code(self.code_with(struct.pack("<I", self.INSIDE)))

    def test_the_second_half_of_a_wide_instruction_is_not_an_instruction(self):
        wide = bytes.fromhex("d5f80448") + bytes.fromhex("0130") * 8 + bytes.fromhex("a3eb0803")
        self.assertEqual(0x0308EBA3, struct.unpack_from("<I", wide, len(wide) - 4)[0])
        self.build_code(self.code_with(wide))

    def test_a_branch_after_a_wide_instruction_is_still_seen(self):
        wide = bytes.fromhex("d5f80448")
        branch = encode_branch(self.HERE + len(wide), self.INSIDE)
        with self.assertRaises(make_image.ImageError):
            self.build_code(self.code_with(wide + branch))

    def test_the_mapping_symbols_of_an_elf_become_regions(self):
        names = b"\x00$t\x00$d.3\x00$x\x00foo\x00"
        table = bytes(16) + b"".join(
            struct.pack("<IIIBBH", name, value, 0, 0, 0, 1)
            for name, value in ((1, 0x03050000), (4, 0x03050101 & ~1 | 0x40), (8, 5), (12, 9), (1, 0x03004000)))
        elf = elf_file([(".code", SHT_PROGBITS, SHF_ALLOC, 0x03050000, bytes(0x200), 0x200),
                        (".symtab", 2, 0, 0, table, len(table)),
                        (".strtab", SHT_STRTAB, 0, 0, names, len(names))])
        self.assertEqual(((0x03050000, "t"), (0x03050140, "d")), make_image.mapping_regions(elf))

    def test_symbols_in_the_region_must_come_from_np_faces(self):
        good = (0x0308BA10, 4, "NOW_PLAYING_FACES", "/w/src/np_faces/np_faces.c")
        make_image.check_np_faces_symbols([good])
        with self.assertRaises(make_image.ImageError):
            make_image.check_np_faces_symbols(
                [good, (0x0308BB00, 8, "helper.isra.0", "/w/src/gfx/canvas.c")])

    def test_a_table_nm_cannot_locate_is_not_judged(self):
        make_image.check_np_faces_symbols([(0x0308BB00, 8, "CASSETTE_CHROME", "")])

    def test_np_faces_code_in_the_window_is_refused(self):
        with self.assertRaises(make_image.ImageError):
            make_image.check_np_faces_symbols(
                [(0x03055000, 8, "face_helper", "/w/src/np_faces/face_text.c")])
        make_image.check_np_faces_symbols(
            [(0x03055000, 8, "canvas_fill_rect", "/w/src/gfx/canvas.c")])

    def test_the_sanctioned_ranges_are_the_loader_symbols(self):
        rows = [(0x03055000, 0x40, "hal_system_load_faces", "/w/platform/device/board/overlay.c"),
                (0x03056000, 0x40, "other", "/w/src/app/app.c"),
                (0x03057000, 0x10, "NP_FACES_OVERLAY", ""),
                (0x03058000, 0x10, "NP_FACES_OVERLAY_COPY", "")]
        self.assertEqual(((0x03055000, 0x03055040), (0x03057000, 0x03057010)),
                         make_image.sanctioned_ranges(rows))

    def test_the_nm_rows_are_parsed_with_their_sources(self):
        output = ("0308ba11 00000004 T draw\t/w/src/np_faces/np_faces.c:12\n"
                  "0308ba10 R no_size\n")
        with mock_nm(output):
            rows = make_image.run_nm("x.elf", "nm")
        self.assertEqual([(0x0308BA10, 4, "draw", "/w/src/np_faces/np_faces.c"),
                          (0x0308BA10, 0, "no_size", "")], rows)

    def test_main_checks_symbols_when_the_elf_has_faces(self):
        parts = firmware(np_faces=bytes(16) + b"\x5A" * 8)
        section = (".np_faces", SHT_PROGBITS, SHF_ALLOC, 0x0308B000, parts.np_faces,
                   len(parts.np_faces))
        with tempfile.TemporaryDirectory() as directory:
            elf = Path(directory) / "echo.elf"
            elf.write_bytes(firmware_elf(parts, [section]))
            arguments = ["--elf", str(elf), "--output", str(Path(directory) / "out.img"),
                         "--build-tag", "t", "--date", "2026-10-22"]
            with mock_nm("0308ba10 00000004 D bad\t/w/src/gfx/canvas.c:1\n"):
                self.assertEqual(1, make_image.main(arguments))
            with mock_nm("0308ba10 00000004 D ok\t/w/src/np_faces/np_faces.c:1\n"):
                self.assertEqual(0, make_image.main(arguments))


class Core1ModuleTest(unittest.TestCase):
    def test_the_mp3_program_follows_its_header_sector(self):
        program = bytes(range(256)) * 3
        image = build(core1_mp3=program)
        offset = make_image.CORE1_MP3_OFFSET
        self.assertEqual(b"EC1M", image[offset:offset + 4])
        self.assertEqual(len(program), word(image, offset + 4))
        self.assertEqual(make_image.crc32(program), word(image, offset + 8))
        start = offset + make_image.CORE1_MODULE_HEADER_SIZE
        self.assertEqual(program, image[start:start + len(program)])
        self.assertEqual([], make_image.validate_image(image))

    def test_the_mp3_program_sits_after_the_overlay_space(self):
        self.assertGreaterEqual(make_image.CORE1_MP3_OFFSET,
                                make_image.OVERLAY_OFFSET + make_image.OVERLAY_CAPACITY)
        self.assertEqual(0, make_image.CORE1_MP3_OFFSET % 512)

    def test_a_program_bigger_than_core_1_code_ram_is_refused(self):
        with self.assertRaises(make_image.ImageError):
            build(core1_mp3=bytes(make_image.CORE1_MODULE_CAPACITY + 1))

    def test_the_flac_program_has_its_own_slot_after_the_mp3_one(self):
        program = bytes(range(200)) * 5
        image = build(core1_flac=program, core1_mp3=b"\x11" * 300)
        offset = make_image.CORE1_FLAC_OFFSET
        self.assertGreaterEqual(offset, make_image.CORE1_MP3_OFFSET +
                                make_image.CORE1_MODULE_HEADER_SIZE +
                                make_image.CORE1_MODULE_CAPACITY)
        self.assertEqual(b"EC1M", image[offset:offset + 4])
        self.assertEqual(len(program), word(image, offset + 4))
        start = offset + make_image.CORE1_MODULE_HEADER_SIZE
        self.assertEqual(program, image[start:start + len(program)])
        self.assertEqual([], make_image.validate_image(image))

    def test_the_aac_program_sits_between_the_flac_slot_and_the_glyph_table(self):
        program = bytes(range(250)) * 7
        image = build(core1_aac=program, core1_flac=b"3" * 300)
        offset = make_image.CORE1_AAC_OFFSET
        self.assertGreaterEqual(offset, make_image.CORE1_FLAC_OFFSET +
                                make_image.CORE1_MODULE_HEADER_SIZE +
                                make_image.CORE1_MODULE_CAPACITY)
        self.assertEqual(0, offset % 512)
        self.assertEqual(make_image.GLYPH_TABLE_OFFSET,
                         offset + make_image.CORE1_MODULE_HEADER_SIZE + make_image.CORE1_AAC_CAPACITY)
        self.assertEqual(b"EC1M", image[offset:offset + 4])
        self.assertEqual(len(program), word(image, offset + 4))
        self.assertEqual(make_image.crc32(program), word(image, offset + 8))
        start = offset + make_image.CORE1_MODULE_HEADER_SIZE
        self.assertEqual(program, image[start:start + len(program)])
        self.assertEqual([], make_image.validate_image(image))

    def test_an_aac_program_that_reaches_the_glyph_table_is_refused(self):
        with self.assertRaises(make_image.ImageError):
            build(core1_aac=bytes(make_image.CORE1_AAC_CAPACITY + 1))

    def test_the_opus_program_sits_between_the_overlay_and_the_mp3_slot(self):
        program = bytes(range(240)) * 9
        image = build(core1_opus=program, core1_mp3=bytes(300))
        offset = make_image.CORE1_OPUS_OFFSET
        self.assertGreaterEqual(offset, make_image.OVERLAY_OFFSET + make_image.OVERLAY_CAPACITY)
        self.assertLessEqual(offset + make_image.CORE1_MODULE_HEADER_SIZE +
                             make_image.CORE1_MODULE_CAPACITY, make_image.CORE1_MP3_OFFSET)
        self.assertEqual(0, offset % 512)
        self.assertEqual(b"EC1M", image[offset:offset + 4])
        self.assertEqual(len(program), word(image, offset + 4))
        start = offset + make_image.CORE1_MODULE_HEADER_SIZE
        self.assertEqual(program, image[start:start + len(program)])
        self.assertEqual([], make_image.validate_image(image))

    def test_no_program_leaves_the_space_empty(self):
        offset = make_image.CORE1_MP3_OFFSET
        self.assertEqual(bytes(16), build()[offset:offset + 16])


class GlyphFaceTest(unittest.TestCase):
    def test_faces_land_where_the_table_says(self):
        faces = [b"\x01" * 700, b"", b"\x02" * 513, b"\x03" * 10]
        image = build(glyph_faces=faces)
        offset = make_image.GLYPH_TABLE_OFFSET
        self.assertEqual(b"EGPT", image[offset:offset + 4])
        self.assertEqual(4, word(image, offset + 4))
        self.assertEqual(faces, make_image.glyph_faces_in(image))
        self.assertEqual([], make_image.validate_image(image))

    def test_faces_start_on_sector_boundaries(self):
        image = build(glyph_faces=[b"\x01" * 700, b"\x02" * 300, b"\x03" * 5])
        table = make_image.GLYPH_TABLE_OFFSET
        for index in range(3):
            self.assertEqual(0, word(image, table + 8 + 8 * index) % 512)

    def test_an_empty_face_has_no_place(self):
        image = build(glyph_faces=[b"", b"\x05" * 8])
        table = make_image.GLYPH_TABLE_OFFSET
        self.assertEqual((0, 0), (word(image, table + 8), word(image, table + 12)))

    def test_faces_stay_clear_of_everything_else(self):
        sizes = [1_720_000, 1_060_000, 35_000, 51_000, 53_000]
        offsets = make_image.glyph_face_offsets(sizes)
        taken = [(make_image.OVERLAY_OFFSET, make_image.OVERLAY_OFFSET + make_image.OVERLAY_CAPACITY),
                 (make_image.CORE1_MP3_OFFSET, make_image.CORE1_MP3_OFFSET + 512 +
                  make_image.CORE1_MODULE_CAPACITY),
                 (make_image.CORE1_FLAC_OFFSET, make_image.CORE1_FLAC_OFFSET + 512 +
                  make_image.CORE1_MODULE_CAPACITY),
                 (make_image.CORE1_AAC_OFFSET, make_image.GLYPH_TABLE_OFFSET),
                 (make_image.CORE1_OPUS_OFFSET, make_image.CORE1_OPUS_OFFSET + 512 +
                  make_image.CORE1_MODULE_CAPACITY),
                 (make_image.GLYPH_TABLE_OFFSET, make_image.GLYPH_TABLE_OFFSET + 512),
                 (make_image.RESIDENT_OFFSET,
                  make_image.RESIDENT_OFFSET + make_image.RESIDENT_CAPACITY),
                 (make_image.NP_FACES_OFFSET,
                  make_image.NP_FACES_OFFSET + make_image.NP_FACES_CAPACITY),
                 (make_image.USB_OVERLAY_OFFSET, make_image.FW_SIZE),
                 (make_image.FW_SIZE, make_image.IMAGE_SIZE),
                 (0, make_image.GLYPH_PAYLOAD_LIMIT)]
        placed = sorted(zip(offsets, sizes))
        for (start, size), (next_start, _) in zip(placed, placed[1:]):
            self.assertLessEqual(start + size, next_start)
        for start, size in placed:
            for low, high in taken:
                self.assertTrue(start + size <= low or start >= high, (start, size, low, high))

    def test_faces_too_big_for_the_image_are_refused(self):
        with self.assertRaises(make_image.ImageError):
            make_image.glyph_face_offsets([2_000_000, 2_000_000])

    def test_no_faces_leave_the_table_empty(self):
        offset = make_image.GLYPH_TABLE_OFFSET
        self.assertEqual(bytes(8), build()[offset:offset + 8])
        self.assertEqual([], make_image.glyph_faces_in(build()))

    def test_the_fw1_end_ignores_the_faces(self):
        self.assertEqual(word(build(), make_image.FW1_END_OFFSET),
                         word(build(glyph_faces=[b"\x01" * 100]), make_image.FW1_END_OFFSET))

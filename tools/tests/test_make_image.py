import datetime
import re
import subprocess
import struct
import sys
import tempfile
import unittest
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
             reset=RESET, code_reset=RESET):
    return make_image.Firmware(
        code=vector_table(reset=code_reset) + code_tail,
        data=struct.pack("<II", stack, reset) + data_tail,
        bss_size=bss_size)


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
        sizes = [1_700_000, 1_100_000, 8_000, 8_000, 8_000]
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

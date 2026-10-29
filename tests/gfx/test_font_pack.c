#include <stddef.h>
#include <string.h>

#include "check.h"
#include "gfx/font_pack.h"
#include "tiny_font.h"

enum {
    GLYPHS = 100,
    FIRST_CODEPOINT = 0x4E00,
    DIRECTORY_OFFSET = FONT_PACK_SECTOR_BYTES,
    INDEX_OFFSET = 2 * FONT_PACK_SECTOR_BYTES,
    INDEX_SECTORS = (GLYPHS + FONT_PACK_ENTRIES_PER_SECTOR - 1) / FONT_PACK_ENTRIES_PER_SECTOR,
    DATA_OFFSET = INDEX_OFFSET + INDEX_SECTORS * FONT_PACK_SECTOR_BYTES,
    GLYPH_WIDTH = 3,
    GLYPH_HEIGHT = 2,
    GLYPH_BYTES = (GLYPH_WIDTH * GLYPH_HEIGHT + 1) / 2,
    PACK_BYTES = DATA_OFFSET + GLYPHS * GLYPH_BYTES,
    MEMORY_BYTES = 16384
};

typedef struct FakePack {
    uint8_t bytes[PACK_BYTES];
    uint32_t reads;
    bool failing;
} FakePack;

static FakePack pack;
static uint32_t memory[MEMORY_BYTES / 4];

static void put16(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *bytes, uint32_t value)
{
    put16(bytes, value);
    put16(bytes + 2, value >> 16);
}

static uint32_t codepoint_of(uint32_t glyph)
{
    return FIRST_CODEPOINT + 2 * glyph;
}

static void build_pack(void)
{
    memset(&pack, 0, sizeof pack);
    uint8_t *header = pack.bytes;
    memcpy(header, "EGLF", 4);
    header[4] = FONT_PACK_VERSION;
    header[5] = FONT_PACK_ENTRY_BYTES;
    put16(header + 6, FONT_PACK_ENTRIES_PER_SECTOR);
    put32(header + 8, GLYPHS);
    put32(header + 12, DIRECTORY_OFFSET);
    put32(header + 16, INDEX_OFFSET);
    put32(header + 20, DATA_OFFSET);
    put32(header + 24, PACK_BYTES);
    for (uint32_t glyph = 0; glyph < GLYPHS; glyph++) {
        uint32_t sector = glyph / FONT_PACK_ENTRIES_PER_SECTOR;
        uint32_t within = glyph % FONT_PACK_ENTRIES_PER_SECTOR;
        if (within == 0) {
            put16(pack.bytes + DIRECTORY_OFFSET + 2 * sector, codepoint_of(glyph));
        }
        uint8_t *entry = pack.bytes + INDEX_OFFSET + sector * FONT_PACK_SECTOR_BYTES +
                         within * FONT_PACK_ENTRY_BYTES;
        put16(entry, codepoint_of(glyph));
        entry[2] = GLYPH_WIDTH;
        entry[3] = GLYPH_HEIGHT;
        entry[4] = 1;
        entry[5] = (uint8_t)-2;
        entry[6] = 9;
        put32(entry + 8, DATA_OFFSET + glyph * GLYPH_BYTES);
        uint8_t *bitmap = pack.bytes + DATA_OFFSET + glyph * GLYPH_BYTES;
        bitmap[0] = (uint8_t)((glyph % 16) << 4 | 15);
    }
}

static bool read_pack(void *context, uint32_t face, uint32_t offset, void *buffer, uint32_t length)
{
    (void)context;
    pack.reads++;
    if (pack.failing || face != 0 || offset > PACK_BYTES || length > PACK_BYTES - offset) {
        return false;
    }
    memcpy(buffer, pack.bytes + offset, length);
    return true;
}

static const FontPackReader READER = {read_pack, NULL};
static Font PACKED_FONT;

static void attach(void)
{
    build_pack();
    font_pack_attach(READER, memory, sizeof memory);
    PACKED_FONT = TINY_FONT;
    PACKED_FONT.pack_faces[1] = 1;
}

static void a_packed_glyph_has_its_metrics(void)
{
    attach();
    const Glyph *glyph = font_pack_find(0, codepoint_of(5));
    CHECK(glyph != NULL);
    if (glyph == NULL) {
        return;
    }
    CHECK_EQUAL(codepoint_of(5), glyph->codepoint);
    CHECK_EQUAL(GLYPH_WIDTH, glyph->width);
    CHECK_EQUAL(GLYPH_HEIGHT, glyph->height);
    CHECK_EQUAL(1, glyph->left);
    CHECK_EQUAL(-2, glyph->top);
    CHECK_EQUAL(9, glyph->advance);
}

static void a_packed_glyph_brings_its_bitmap(void)
{
    attach();
    const Glyph *glyph = font_pack_find(0, codepoint_of(7));
    const uint8_t *bitmap = glyph != NULL ? font_pack_bitmap(glyph) : NULL;
    CHECK(bitmap != NULL);
    if (bitmap == NULL) {
        return;
    }
    CHECK_EQUAL(7, font_bitmap_level(bitmap, glyph, 0, 0));
    CHECK_EQUAL(15, font_bitmap_level(bitmap, glyph, 1, 0));
    CHECK_EQUAL(0, font_bitmap_level(bitmap, glyph, 2, 1));
}

static void glyphs_in_later_index_sectors_are_found(void)
{
    attach();
    for (uint32_t glyph = 0; glyph < GLYPHS; glyph++) {
        const Glyph *found = font_pack_find(0, codepoint_of(glyph));
        CHECK(found != NULL && found->codepoint == codepoint_of(glyph));
    }
}

static void codepoints_outside_the_pack_are_missing(void)
{
    attach();
    CHECK(font_pack_find(0, codepoint_of(3) + 1) == NULL);
    CHECK(font_pack_find(0, FIRST_CODEPOINT - 1) == NULL);
    CHECK(font_pack_find(0, codepoint_of(GLYPHS - 1) + 2) == NULL);
    CHECK(font_pack_find(1, codepoint_of(0)) == NULL);
    CHECK(font_pack_find(FONT_PACK_FACES, codepoint_of(0)) == NULL);
    CHECK(font_pack_find(0, 0x1F600) == NULL);
}

static void a_second_lookup_reads_nothing(void)
{
    attach();
    font_pack_find(0, codepoint_of(10));
    font_pack_find(0, codepoint_of(10) + 1);
    uint32_t reads = pack.reads;
    CHECK(font_pack_find(0, codepoint_of(10)) != NULL);
    CHECK(font_pack_find(0, codepoint_of(10) + 1) == NULL);
    CHECK_EQUAL(reads, pack.reads);
}

static void the_header_is_read_once(void)
{
    attach();
    font_pack_find(0, codepoint_of(1));
    uint32_t reads = pack.reads;
    font_pack_find(0, codepoint_of(2));
    CHECK_EQUAL(reads + 2, pack.reads);
}

static void forgetting_reads_the_pack_again(void)
{
    attach();
    font_pack_find(0, codepoint_of(4));
    uint32_t reads = pack.reads;
    font_pack_forget();
    CHECK(font_pack_find(0, codepoint_of(4)) != NULL);
    CHECK(pack.reads > reads);
}

static void a_pack_without_its_magic_is_ignored(void)
{
    attach();
    pack.bytes[0] = 'X';
    CHECK(font_pack_find(0, codepoint_of(0)) == NULL);
}

static void a_pack_of_another_version_is_ignored(void)
{
    attach();
    pack.bytes[4] = FONT_PACK_VERSION + 1;
    CHECK(font_pack_find(0, codepoint_of(0)) == NULL);
}

static void a_failed_read_finds_nothing(void)
{
    attach();
    pack.failing = true;
    CHECK(font_pack_find(0, codepoint_of(0)) == NULL);
}

static void too_little_memory_keeps_packs_off(void)
{
    build_pack();
    CHECK_EQUAL(0, font_pack_attach(READER, memory, 64));
    CHECK(font_pack_find(0, codepoint_of(0)) == NULL);
}

static void the_memory_holds_whole_sets_of_slots(void)
{
    build_pack();
    uint32_t slots = font_pack_attach(READER, memory, sizeof memory);
    CHECK(slots >= FONT_PACK_WAYS);
    CHECK_EQUAL(0, slots % FONT_PACK_WAYS);
    uint32_t sets = slots / FONT_PACK_WAYS;
    CHECK_EQUAL(0, sets & (sets - 1));
}

static void a_font_falls_back_to_its_pack(void)
{
    attach();
    const Glyph *glyph = font_find_glyph(&PACKED_FONT, codepoint_of(2));
    CHECK(glyph != NULL && glyph->codepoint == codepoint_of(2));
    if (glyph != NULL) {
        CHECK_EQUAL(2, font_glyph_level(&PACKED_FONT, glyph, 0, 0));
    }
    const Glyph *letter = font_find_glyph(&PACKED_FONT, 'A');
    CHECK(letter != NULL && letter->codepoint == 'A');
    if (letter != NULL) {
        CHECK_EQUAL(15, font_glyph_level(&PACKED_FONT, letter, 0, 0));
    }
}

static void a_character_in_neither_falls_back_to_the_question_mark(void)
{
    attach();
    const Glyph *glyph = font_find_glyph(&PACKED_FONT, codepoint_of(2) + 1);
    CHECK(glyph != NULL && glyph->codepoint == FONT_FALLBACK_CODEPOINT);
}

static void without_packs_a_font_uses_the_question_mark(void)
{
    attach();
    font_pack_detach();
    const Glyph *glyph = font_find_glyph(&PACKED_FONT, codepoint_of(2));
    CHECK(glyph != NULL && glyph->codepoint == FONT_FALLBACK_CODEPOINT);
}

static void characters_below_cjk_use_the_other_face(void)
{
    attach();
    PACKED_FONT.pack_faces[0] = 2;
    PACKED_FONT.pack_faces[1] = 0;
    const Glyph *glyph = font_find_glyph(&PACKED_FONT, codepoint_of(2));
    CHECK(glyph != NULL && glyph->codepoint == FONT_FALLBACK_CODEPOINT);
    uint32_t reads = pack.reads;
    CHECK(font_find_glyph(&PACKED_FONT, 0xE9) != NULL);
    CHECK(pack.reads > reads);
}

static void a_font_without_a_pack_face_ignores_packs(void)
{
    attach();
    const Glyph *glyph = font_find_glyph(&TINY_FONT, codepoint_of(2));
    CHECK(glyph != NULL && glyph->codepoint == FONT_FALLBACK_CODEPOINT);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_packed_glyph_has_its_metrics),
        TEST_CASE(a_packed_glyph_brings_its_bitmap),
        TEST_CASE(glyphs_in_later_index_sectors_are_found),
        TEST_CASE(codepoints_outside_the_pack_are_missing),
        TEST_CASE(a_second_lookup_reads_nothing),
        TEST_CASE(the_header_is_read_once),
        TEST_CASE(forgetting_reads_the_pack_again),
        TEST_CASE(a_pack_without_its_magic_is_ignored),
        TEST_CASE(a_pack_of_another_version_is_ignored),
        TEST_CASE(a_failed_read_finds_nothing),
        TEST_CASE(too_little_memory_keeps_packs_off),
        TEST_CASE(the_memory_holds_whole_sets_of_slots),
        TEST_CASE(a_font_falls_back_to_its_pack),
        TEST_CASE(a_character_in_neither_falls_back_to_the_question_mark),
        TEST_CASE(without_packs_a_font_uses_the_question_mark),
        TEST_CASE(characters_below_cjk_use_the_other_face),
        TEST_CASE(a_font_without_a_pack_face_ignores_packs),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

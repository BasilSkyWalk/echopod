#include <stddef.h>

#include "check.h"
#include "fonts/fonts.h"

static const Font *const ALL_FONTS[] = {
    &FONT_INTER_REGULAR_12,
    &FONT_INTER_REGULAR_16,
    &FONT_INTER_SEMIBOLD_16,
};

enum {
    FONT_COUNT = sizeof ALL_FONTS / sizeof ALL_FONTS[0]
};

static void every_font_covers_printable_ascii(void)
{
    for (size_t font = 0; font < FONT_COUNT; font++) {
        for (uint32_t codepoint = ' '; codepoint <= '~'; codepoint++) {
            const Glyph *glyph = font_find_glyph(ALL_FONTS[font], codepoint);
            CHECK(glyph != NULL && glyph->codepoint == codepoint);
        }
    }
}

static void every_font_builds_in_its_punctuation(void)
{
    const uint32_t built_in[] = {0x2026, 0x2019, 0x203A, 0x2605, 0x2713};
    for (size_t font = 0; font < FONT_COUNT; font++) {
        for (size_t index = 0; index < sizeof built_in / sizeof built_in[0]; index++) {
            const Glyph *glyph = font_find_glyph(ALL_FONTS[font], built_in[index]);
            CHECK(glyph != NULL && glyph->codepoint == built_in[index]);
        }
    }
}

static void every_font_sends_accents_to_a_pack(void)
{
    for (size_t font = 0; font < FONT_COUNT; font++) {
        CHECK(ALL_FONTS[font]->pack_faces[0] != 0);
        CHECK(ALL_FONTS[font]->pack_faces[1] != 0);
        CHECK(ALL_FONTS[font]->pack_faces[0] != ALL_FONTS[font]->pack_faces[1]);
    }
}

static void glyphs_are_sorted_by_codepoint(void)
{
    for (size_t font = 0; font < FONT_COUNT; font++) {
        const Font *current = ALL_FONTS[font];
        for (uint16_t index = 1; index < current->glyph_count; index++) {
            CHECK(current->glyphs[index - 1].codepoint < current->glyphs[index].codepoint);
        }
    }
}

static void glyph_bitmaps_stay_inside_the_font_bitmap(void)
{
    for (size_t font = 0; font < FONT_COUNT; font++) {
        const Font *current = ALL_FONTS[font];
        for (uint16_t index = 0; index < current->glyph_count; index++) {
            const Glyph *glyph = &current->glyphs[index];
            int packed_bytes = (glyph->width * glyph->height + 1) / 2;
            CHECK(glyph->bitmap_offset + packed_bytes <= current->bitmap_size);
        }
    }
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(every_font_covers_printable_ascii),
        TEST_CASE(every_font_builds_in_its_punctuation),
        TEST_CASE(every_font_sends_accents_to_a_pack),
        TEST_CASE(glyphs_are_sorted_by_codepoint),
        TEST_CASE(glyph_bitmaps_stay_inside_the_font_bitmap),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

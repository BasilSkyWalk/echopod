#include <stddef.h>

#include "check.h"
#include "fonts/fonts.h"
#include "gfx/font_pack.h"
#include "sim_glyphs.h"

static const Font *const ALL_FONTS[] = {
    &FONT_INTER_REGULAR_12,
    &FONT_INTER_REGULAR_16,
    &FONT_INTER_SEMIBOLD_16,
};

enum {
    FONT_COUNT = sizeof ALL_FONTS / sizeof ALL_FONTS[0]
};

static void the_committed_accent_packs_load(void)
{
    CHECK(sim_glyphs_face_count() >= 3);
}

static void every_accented_letter_comes_from_a_pack(void)
{
    for (size_t font = 0; font < FONT_COUNT; font++) {
        for (uint32_t codepoint = 0xC0; codepoint <= 0xFF; codepoint++) {
            const Glyph *glyph = font_find_glyph(ALL_FONTS[font], codepoint);
            CHECK(glyph != NULL && glyph->codepoint == codepoint);
            CHECK(glyph != NULL && font_pack_bitmap(glyph) != NULL);
        }
    }
}

static void each_font_has_its_own_accents(void)
{
    const Glyph *regular = font_find_glyph(&FONT_INTER_REGULAR_16, 0xE9);
    int regular_advance = regular != NULL ? regular->advance : -1;
    int regular_width = regular != NULL ? regular->width : -1;
    const Glyph *small = font_find_glyph(&FONT_INTER_REGULAR_12, 0xE9);
    CHECK(small != NULL && small->width < regular_width);
    const Glyph *bold = font_find_glyph(&FONT_INTER_SEMIBOLD_16, 0xE9);
    CHECK(bold != NULL && bold->codepoint == 0xE9);
    CHECK(regular_advance > 0);
}

static void the_no_break_space_is_blank(void)
{
    for (size_t font = 0; font < FONT_COUNT; font++) {
        const Glyph *glyph = font_find_glyph(ALL_FONTS[font], 0xA0);
        CHECK(glyph != NULL && glyph->codepoint == 0xA0 && glyph->width == 0 && glyph->advance > 0);
    }
}

int main(void)
{
    sim_glyphs_attach();
    static const TestCase cases[] = {
        TEST_CASE(the_committed_accent_packs_load),
        TEST_CASE(every_accented_letter_comes_from_a_pack),
        TEST_CASE(each_font_has_its_own_accents),
        TEST_CASE(the_no_break_space_is_blank),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

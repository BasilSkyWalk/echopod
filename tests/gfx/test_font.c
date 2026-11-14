#include <stddef.h>

#include "check.h"
#include "tiny_font.h"

static void a_glyph_is_found_by_its_codepoint(void)
{
    const Glyph *glyph = font_find_glyph(&TINY_FONT, 'B');
    CHECK(glyph != NULL);
    CHECK_EQUAL('B', glyph != NULL ? glyph->codepoint : 0);
}

static void a_missing_codepoint_falls_back_to_the_question_mark(void)
{
    const Glyph *glyph = font_find_glyph(&TINY_FONT, 0x4E00);
    CHECK(glyph != NULL);
    CHECK_EQUAL(FONT_FALLBACK_CODEPOINT, glyph != NULL ? glyph->codepoint : 0);
}

static void a_font_without_a_fallback_finds_nothing_for_missing_codepoints(void)
{
    CHECK(font_find_glyph(&TINY_FONT_WITHOUT_FALLBACK, 'Z') == NULL);
    CHECK(font_find_glyph(&TINY_FONT_WITHOUT_FALLBACK, 'A') != NULL);
}

static void levels_are_read_row_by_row_high_nibble_first(void)
{
    const Glyph *letter_a = font_find_glyph(&TINY_FONT, 'A');
    CHECK_EQUAL(15, font_glyph_level(&TINY_FONT, letter_a, 0, 0));
    CHECK_EQUAL(0, font_glyph_level(&TINY_FONT, letter_a, 1, 0));
    CHECK_EQUAL(0, font_glyph_level(&TINY_FONT, letter_a, 0, 1));
    CHECK_EQUAL(15, font_glyph_level(&TINY_FONT, letter_a, 1, 1));
    const Glyph *letter_b = font_find_glyph(&TINY_FONT, 'B');
    CHECK_EQUAL(4, font_glyph_level(&TINY_FONT, letter_b, 0, 0));
    CHECK_EQUAL(15, font_glyph_level(&TINY_FONT, letter_b, 1, 0));
}

static void line_height_spans_ascent_and_descent(void)
{
    CHECK_EQUAL(3, font_line_height(&TINY_FONT));
}

static const Glyph OWN_GLYPHS[] = {{'B', 0, 2, 1, 0, 1, 3}};
static const uint8_t OWN_BITMAP[] = {0x7F};
static const Font OWN_FONT = {.glyphs = OWN_GLYPHS,
                              .bitmap = OWN_BITMAP,
                              .glyph_count = 1,
                              .bitmap_size = 1,
                              .ascent = 2,
                              .descent = 1,
                              .fallback = &TINY_FONT};
static const Font EMPTY_FONT = {.ascent = 2, .descent = 1, .fallback = &TINY_FONT};

static void a_font_with_a_fallback_finds_its_own_glyphs_first(void)
{
    const Glyph *own = font_find_glyph(&OWN_FONT, 'B');
    CHECK(own == &OWN_GLYPHS[0]);
    CHECK_EQUAL(7, font_glyph_level(&OWN_FONT, own, 0, 0));
    CHECK_EQUAL(15, font_glyph_level(&OWN_FONT, own, 1, 0));
}

static void a_codepoint_the_font_lacks_is_drawn_from_its_fallback(void)
{
    const Glyph *borrowed = font_find_glyph(&OWN_FONT, 'A');
    CHECK(borrowed == font_find_glyph(&TINY_FONT, 'A'));
    CHECK_EQUAL(15, font_glyph_level(&OWN_FONT, borrowed, 0, 0));
    CHECK_EQUAL(0, font_glyph_level(&OWN_FONT, borrowed, 1, 0));
    CHECK(font_find_glyph(&OWN_FONT, 0x4E00) == font_find_glyph(&TINY_FONT, 0x4E00));
}

static void a_font_with_no_glyphs_hands_everything_to_its_fallback(void)
{
    CHECK(font_find_glyph(&EMPTY_FONT, 'B') == font_find_glyph(&TINY_FONT, 'B'));
    const Glyph *letter_b = font_find_glyph(&EMPTY_FONT, 'B');
    CHECK_EQUAL(4, font_glyph_level(&EMPTY_FONT, letter_b, 0, 0));
    CHECK_EQUAL(3, font_line_height(&EMPTY_FONT));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_glyph_is_found_by_its_codepoint),
        TEST_CASE(a_missing_codepoint_falls_back_to_the_question_mark),
        TEST_CASE(a_font_without_a_fallback_finds_nothing_for_missing_codepoints),
        TEST_CASE(levels_are_read_row_by_row_high_nibble_first),
        TEST_CASE(line_height_spans_ascent_and_descent),
        TEST_CASE(a_font_with_a_fallback_finds_its_own_glyphs_first),
        TEST_CASE(a_codepoint_the_font_lacks_is_drawn_from_its_fallback),
        TEST_CASE(a_font_with_no_glyphs_hands_everything_to_its_fallback),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

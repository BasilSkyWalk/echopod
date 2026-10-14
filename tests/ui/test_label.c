#include "check.h"
#include "tiny_font.h"
#include "ui/label.h"

enum {
    WIDTH = 10,
    HEIGHT = 4,
    BACKGROUND = 0x0000,
    INK = 0xFFFF
};

static Color pixels[WIDTH * HEIGHT];

static const TextStyle INK_STYLE = {.font = &TINY_FONT, .color = INK};

static Canvas blank_canvas(void)
{
    Canvas canvas = canvas_make(pixels, (Rect){0, 0, WIDTH, HEIGHT});
    canvas_clear(&canvas, BACKGROUND);
    return canvas;
}

static void text_that_fits_is_kept_whole(void)
{
    LabelFit fit = label_fit(&TINY_FONT, "AB", 7);
    CHECK_EQUAL(2, fit.byte_count);
    CHECK(!fit.ellipsis);
}

static void text_too_wide_is_cut_to_make_room_for_the_ellipsis(void)
{
    LabelFit fit = label_fit(&TINY_FONT, "AB", 6);
    CHECK_EQUAL(1, fit.byte_count);
    CHECK(fit.ellipsis);
}

static void the_cut_never_splits_a_character(void)
{
    LabelFit fit = label_fit(&TINY_FONT,
                             "\xC3\xA9"
                             "A",
                             5);
    CHECK_EQUAL(2, fit.byte_count);
    CHECK(fit.ellipsis);
}

static void a_width_too_small_for_any_character_keeps_only_the_ellipsis(void)
{
    LabelFit fit = label_fit(&TINY_FONT, "AB", 3);
    CHECK_EQUAL(0, fit.byte_count);
    CHECK(fit.ellipsis);
}

static void the_baseline_centres_the_line_in_its_area(void)
{
    CHECK_EQUAL(4, label_baseline(&TINY_FONT, (Rect){0, 0, 10, 7}));
    CHECK_EQUAL(12, label_baseline(&TINY_FONT, (Rect){0, 8, 10, 7}));
}

static void a_label_never_paints_outside_its_area(void)
{
    Canvas canvas = blank_canvas();
    label_draw(&canvas, INK_STYLE, (Label){.area = {2, 0, 4, 4}, .text = "AB"});
    int inked_inside = 0;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            bool inside = x >= 2 && x < 6;
            Color pixel = canvas_pixel_at(&canvas, x, y);
            CHECK(inside || pixel == BACKGROUND);
            inked_inside += inside && pixel != BACKGROUND ? 1 : 0;
        }
    }
    CHECK(inked_inside > 0);
}

static void a_scrolling_label_shifts_its_text_and_repeats_it(void)
{
    Canvas canvas = blank_canvas();
    Label label = {.area = {0, 0, WIDTH, HEIGHT}, .text = "A"};
    label_draw_scrolling(&canvas, INK_STYLE, label, (Marquee){.offset = 1, .gap = 2});
    CHECK_EQUAL(INK, canvas_pixel_at(&canvas, 0, 0));
    CHECK_EQUAL(BACKGROUND, canvas_pixel_at(&canvas, 1, 0));
    CHECK_EQUAL(INK, canvas_pixel_at(&canvas, 6, 0));
}

static void a_centred_area_sits_in_the_middle_of_its_bounds(void)
{
    Rect area = label_centered_area(&TINY_FONT, (Rect){10, 3, 20, 5}, "AB");
    CHECK_EQUAL(16, area.x);
    CHECK_EQUAL(7, area.width);
    CHECK_EQUAL(3, area.y);
    CHECK_EQUAL(5, area.height);
}

static void a_centred_area_is_never_wider_than_its_bounds(void)
{
    Rect area = label_centered_area(&TINY_FONT, (Rect){10, 3, 5, 5}, "AB");
    CHECK_EQUAL(10, area.x);
    CHECK_EQUAL(5, area.width);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(text_that_fits_is_kept_whole),
        TEST_CASE(text_too_wide_is_cut_to_make_room_for_the_ellipsis),
        TEST_CASE(the_cut_never_splits_a_character),
        TEST_CASE(a_width_too_small_for_any_character_keeps_only_the_ellipsis),
        TEST_CASE(the_baseline_centres_the_line_in_its_area),
        TEST_CASE(a_label_never_paints_outside_its_area),
        TEST_CASE(a_scrolling_label_shifts_its_text_and_repeats_it),
        TEST_CASE(a_centred_area_sits_in_the_middle_of_its_bounds),
        TEST_CASE(a_centred_area_is_never_wider_than_its_bounds),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

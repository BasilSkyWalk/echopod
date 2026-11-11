#include "check.h"
#include "ui/box.h"
#include "ui_test_canvas.h"
#include "ui_test_theme.h"

enum {
    UNTOUCHED = 0x0821
};

static const Rect AREA = {4, 3, 20, 10};

static Canvas drawn(const BoxStyle *style)
{
    Canvas canvas = ui_test_canvas();
    box_draw(&canvas, &TEST_COLORS, style, AREA);
    return canvas;
}

static void a_flat_box_fills_exactly_its_area(void)
{
    BoxStyle style = {.fill_top = THEME_COLOR_DANGER, .fill_bottom = THEME_COLOR_DANGER};
    Canvas canvas = drawn(&style);
    Rect above = {0, 0, TEST_SCREEN_WIDTH, AREA.y};
    Rect right = {AREA.x + AREA.width, 0, TEST_SCREEN_WIDTH - AREA.x - AREA.width,
                  TEST_SCREEN_HEIGHT};
    CHECK(ui_test_area_has_only(&canvas, AREA, TEST_COLORS.danger));
    CHECK(ui_test_area_has_only(&canvas, above, UNTOUCHED));
    CHECK(ui_test_area_has_only(&canvas, right, UNTOUCHED));
}

static void a_box_shades_from_its_top_colour_to_its_bottom_colour(void)
{
    BoxStyle style = {.fill_top = THEME_COLOR_TEXT, .fill_bottom = THEME_COLOR_DANGER};
    Canvas canvas = drawn(&style);
    Color middle = canvas_pixel_at(&canvas, AREA.x, AREA.y + AREA.height / 2);
    CHECK_EQUAL(TEST_COLORS.text, canvas_pixel_at(&canvas, AREA.x, AREA.y));
    CHECK_EQUAL(TEST_COLORS.danger, canvas_pixel_at(&canvas, AREA.x, AREA.y + AREA.height - 1));
    CHECK(middle != TEST_COLORS.text && middle != TEST_COLORS.danger);
}

static void a_shine_line_sits_above_the_fill(void)
{
    BoxStyle style = {.fill_top = THEME_COLOR_DANGER,
                      .fill_bottom = THEME_COLOR_DANGER,
                      .shine = THEME_COLOR_SUCCESS,
                      .shine_height = 2};
    Canvas canvas = drawn(&style);
    Rect shine = {AREA.x, AREA.y, AREA.width, 2};
    Rect body = {AREA.x, AREA.y + 2, AREA.width, AREA.height - 2};
    CHECK(ui_test_area_has_only(&canvas, shine, TEST_COLORS.success));
    CHECK(ui_test_area_has_only(&canvas, body, TEST_COLORS.danger));
}

static void a_border_surrounds_the_fill(void)
{
    BoxStyle style = {.fill_top = THEME_COLOR_DANGER,
                      .fill_bottom = THEME_COLOR_DANGER,
                      .border = THEME_COLOR_SUCCESS,
                      .border_width = 2,
                      .border_edges = BOX_EDGE_ALL};
    Canvas canvas = drawn(&style);
    Rect inner = {AREA.x + 2, AREA.y + 2, AREA.width - 4, AREA.height - 4};
    Rect left = {AREA.x, AREA.y, 2, AREA.height};
    Rect bottom = {AREA.x, AREA.y + AREA.height - 2, AREA.width, 2};
    CHECK(ui_test_area_has_only(&canvas, inner, TEST_COLORS.danger));
    CHECK(ui_test_area_has_only(&canvas, left, TEST_COLORS.success));
    CHECK(ui_test_area_has_only(&canvas, bottom, TEST_COLORS.success));
}

static void a_border_is_drawn_only_on_its_edges(void)
{
    BoxStyle style = {.fill_top = THEME_COLOR_DANGER,
                      .fill_bottom = THEME_COLOR_DANGER,
                      .border = THEME_COLOR_SUCCESS,
                      .border_width = 1,
                      .border_edges = BOX_EDGE_BOTTOM};
    Canvas canvas = drawn(&style);
    Rect body = {AREA.x, AREA.y, AREA.width, AREA.height - 1};
    Rect bottom = {AREA.x, AREA.y + AREA.height - 1, AREA.width, 1};
    CHECK(ui_test_area_has_only(&canvas, body, TEST_COLORS.danger));
    CHECK(ui_test_area_has_only(&canvas, bottom, TEST_COLORS.success));
}

static void an_inset_box_leaves_a_margin_inside_its_area(void)
{
    BoxStyle style = {.fill_top = THEME_COLOR_DANGER,
                      .fill_bottom = THEME_COLOR_DANGER,
                      .inset_across = 3,
                      .inset_down = 1};
    Canvas canvas = drawn(&style);
    Rect box = {AREA.x + 3, AREA.y + 1, AREA.width - 6, AREA.height - 2};
    Rect left = {AREA.x, AREA.y, 3, AREA.height};
    Rect top = {AREA.x, AREA.y, AREA.width, 1};
    CHECK(ui_test_area_has_only(&canvas, box, TEST_COLORS.danger));
    CHECK(ui_test_area_has_only(&canvas, left, UNTOUCHED));
    CHECK(ui_test_area_has_only(&canvas, top, UNTOUCHED));
}

static void a_box_stays_inside_the_clip(void)
{
    BoxStyle style = {.fill_top = THEME_COLOR_DANGER,
                      .fill_bottom = THEME_COLOR_SUCCESS,
                      .border = THEME_COLOR_TEXT,
                      .border_width = 1,
                      .border_edges = BOX_EDGE_ALL};
    Canvas canvas = ui_test_canvas();
    Rect strip = {0, AREA.y + 2, TEST_SCREEN_WIDTH, 3};
    Canvas clipped = canvas_clipped(&canvas, strip);
    box_draw(&clipped, &TEST_COLORS, &style, AREA);
    Rect above = {0, 0, TEST_SCREEN_WIDTH, strip.y};
    Rect below = {0, strip.y + strip.height, TEST_SCREEN_WIDTH,
                  TEST_SCREEN_HEIGHT - strip.y - strip.height};
    CHECK(ui_test_area_has_only(&canvas, above, UNTOUCHED));
    CHECK(ui_test_area_has_only(&canvas, below, UNTOUCHED));
    CHECK_EQUAL(TEST_COLORS.text, canvas_pixel_at(&canvas, AREA.x, strip.y));
}

static void a_rounded_box_leaves_its_corner_pixels_alone(void)
{
    BoxStyle style = {
        .fill_top = THEME_COLOR_DANGER, .fill_bottom = THEME_COLOR_DANGER, .radius = 3};
    Canvas canvas = drawn(&style);
    int right = AREA.x + AREA.width - 1;
    int bottom = AREA.y + AREA.height - 1;
    CHECK_EQUAL(UNTOUCHED, canvas_pixel_at(&canvas, AREA.x, AREA.y));
    CHECK_EQUAL(UNTOUCHED, canvas_pixel_at(&canvas, right, AREA.y));
    CHECK_EQUAL(UNTOUCHED, canvas_pixel_at(&canvas, AREA.x, bottom));
    CHECK_EQUAL(UNTOUCHED, canvas_pixel_at(&canvas, right, bottom));
    CHECK_EQUAL(TEST_COLORS.danger, canvas_pixel_at(&canvas, AREA.x + AREA.width / 2, AREA.y));
    CHECK_EQUAL(TEST_COLORS.danger, canvas_pixel_at(&canvas, AREA.x, AREA.y + AREA.height / 2));
}

static void a_rounded_corner_blends_its_edge_the_same_on_every_corner(void)
{
    BoxStyle style = {
        .fill_top = THEME_COLOR_DANGER, .fill_bottom = THEME_COLOR_DANGER, .radius = 3};
    Canvas canvas = drawn(&style);
    int right = AREA.x + AREA.width - 1;
    int bottom = AREA.y + AREA.height - 1;
    Color edge = canvas_pixel_at(&canvas, AREA.x + 1, AREA.y);
    CHECK(edge != UNTOUCHED && edge != TEST_COLORS.danger);
    CHECK_EQUAL(edge, canvas_pixel_at(&canvas, right - 1, AREA.y));
    CHECK_EQUAL(edge, canvas_pixel_at(&canvas, AREA.x + 1, bottom));
    CHECK_EQUAL(edge, canvas_pixel_at(&canvas, right - 1, bottom));
}

static void a_radius_is_limited_to_half_the_shorter_side(void)
{
    BoxStyle style = {
        .fill_top = THEME_COLOR_DANGER, .fill_bottom = THEME_COLOR_DANGER, .radius = 200};
    Canvas canvas = drawn(&style);
    Rect below = {0, AREA.y + AREA.height, TEST_SCREEN_WIDTH,
                  TEST_SCREEN_HEIGHT - AREA.y - AREA.height};
    Rect left = {0, 0, AREA.x, TEST_SCREEN_HEIGHT};
    Rect middle = {AREA.x + AREA.height / 2, AREA.y, AREA.width - AREA.height, AREA.height};
    CHECK(ui_test_area_has_only(&canvas, below, UNTOUCHED));
    CHECK(ui_test_area_has_only(&canvas, left, UNTOUCHED));
    CHECK(ui_test_area_has_only(&canvas, middle, TEST_COLORS.danger));
}

static void a_rounded_border_rings_a_rounded_fill(void)
{
    BoxStyle style = {.fill_top = THEME_COLOR_DANGER,
                      .fill_bottom = THEME_COLOR_DANGER,
                      .border = THEME_COLOR_SUCCESS,
                      .border_width = 1,
                      .border_edges = BOX_EDGE_ALL,
                      .radius = 3};
    Canvas canvas = drawn(&style);
    int across = AREA.x + AREA.width / 2;
    CHECK_EQUAL(UNTOUCHED, canvas_pixel_at(&canvas, AREA.x, AREA.y));
    CHECK_EQUAL(TEST_COLORS.success, canvas_pixel_at(&canvas, across, AREA.y));
    CHECK_EQUAL(TEST_COLORS.success, canvas_pixel_at(&canvas, AREA.x, AREA.y + AREA.height / 2));
    CHECK_EQUAL(TEST_COLORS.danger, canvas_pixel_at(&canvas, across, AREA.y + 1));
}

static void a_rounded_box_stays_inside_the_clip(void)
{
    BoxStyle style = {
        .fill_top = THEME_COLOR_DANGER, .fill_bottom = THEME_COLOR_SUCCESS, .radius = 4};
    Canvas canvas = ui_test_canvas();
    Rect strip = {0, AREA.y + 1, TEST_SCREEN_WIDTH, 2};
    Canvas clipped = canvas_clipped(&canvas, strip);
    box_draw(&clipped, &TEST_COLORS, &style, AREA);
    Rect above = {0, 0, TEST_SCREEN_WIDTH, strip.y};
    Rect below = {0, strip.y + strip.height, TEST_SCREEN_WIDTH,
                  TEST_SCREEN_HEIGHT - strip.y - strip.height};
    CHECK(ui_test_area_has_only(&canvas, above, UNTOUCHED));
    CHECK(ui_test_area_has_only(&canvas, below, UNTOUCHED));
    CHECK(canvas_pixel_at(&canvas, AREA.x + AREA.width / 2, strip.y) != UNTOUCHED);
}

static void the_standard_selection_shapes_are_the_first_three_boxes(void)
{
    Theme theme = TEST_THEME;
    theme.selection = THEME_SELECTION_GRADIENT;
    CHECK(theme_selection_box(&theme) == &THEME_STANDARD_BOXES.selection);
    theme.selection = THEME_SELECTION_FLAT;
    CHECK(theme_selection_box(&theme) == &THEME_STANDARD_BOXES.selection_flat);
    theme.selection = THEME_SELECTION_OUTLINE;
    CHECK(theme_selection_box(&theme) == &THEME_STANDARD_BOXES.selection_outline);
}

static void a_colour_is_found_by_its_id(void)
{
    CHECK_EQUAL(TEST_COLORS.background, theme_color(&TEST_COLORS, THEME_COLOR_BACKGROUND));
    CHECK_EQUAL(TEST_COLORS.danger, theme_color(&TEST_COLORS, THEME_COLOR_DANGER));
    CHECK_EQUAL(TEST_COLORS.chevron, theme_color(&TEST_COLORS, THEME_COLOR_CHEVRON));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_flat_box_fills_exactly_its_area),
        TEST_CASE(a_box_shades_from_its_top_colour_to_its_bottom_colour),
        TEST_CASE(a_shine_line_sits_above_the_fill),
        TEST_CASE(a_border_surrounds_the_fill),
        TEST_CASE(a_border_is_drawn_only_on_its_edges),
        TEST_CASE(an_inset_box_leaves_a_margin_inside_its_area),
        TEST_CASE(a_box_stays_inside_the_clip),
        TEST_CASE(a_rounded_box_leaves_its_corner_pixels_alone),
        TEST_CASE(a_rounded_corner_blends_its_edge_the_same_on_every_corner),
        TEST_CASE(a_radius_is_limited_to_half_the_shorter_side),
        TEST_CASE(a_rounded_border_rings_a_rounded_fill),
        TEST_CASE(a_rounded_box_stays_inside_the_clip),
        TEST_CASE(the_standard_selection_shapes_are_the_first_three_boxes),
        TEST_CASE(a_colour_is_found_by_its_id),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

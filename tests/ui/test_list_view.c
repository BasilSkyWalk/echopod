#include "check.h"
#include "ui/list_view.h"
#include "ui_test_canvas.h"
#include "ui_test_theme.h"

typedef struct FakeItems {
    int count;
    const char *detail_at_one;
    bool opens_at_one;
} FakeItems;

static int fake_count(const void *context)
{
    const FakeItems *items = context;
    return items->count;
}

static const char *fake_label(const void *context, int index)
{
    (void)context;
    return index % 2 == 0 ? "A" : "B";
}

static const char *fake_detail(const void *context, int index)
{
    const FakeItems *items = context;
    return index == 1 ? items->detail_at_one : NULL;
}

static bool fake_opens(const void *context, int index)
{
    const FakeItems *items = context;
    return index == 1 && items->opens_at_one;
}

static FakeItems items;
static ListModel model;

static ListView list_of(int count)
{
    items = (FakeItems){.count = count, .detail_at_one = NULL};
    model = (ListModel){.context = &items,
                        .count = fake_count,
                        .label = fake_label,
                        .detail = fake_detail,
                        .opens = fake_opens};
    ListView view;
    list_view_init(&view, &model, &TEST_THEME, 0);
    return view;
}

static void move_times(ListView *view, int delta, int times)
{
    for (int step = 0; step < times; step++) {
        list_view_move(view, delta, false, 100);
    }
}

static Rect row_rect(int row)
{
    Rect content = theme_content_area(&TEST_THEME);
    return (Rect){0, content.y + row * TEST_ROW_HEIGHT, 1, TEST_ROW_HEIGHT};
}

static void a_new_list_selects_the_first_item(void)
{
    ListView view = list_of(10);
    CHECK_EQUAL(0, view.selected);
    CHECK_EQUAL(0, view.top);
    CHECK_EQUAL(TEST_VISIBLE_ROWS, list_view_rows(&view));
}

static void moving_within_the_page_keeps_the_top(void)
{
    ListView view = list_of(10);
    move_times(&view, 1, 2);
    CHECK_EQUAL(2, view.selected);
    CHECK_EQUAL(0, view.top);
}

static void moving_past_the_last_visible_row_scrolls_down(void)
{
    ListView view = list_of(10);
    move_times(&view, 1, 3);
    CHECK_EQUAL(3, view.selected);
    CHECK_EQUAL(1, view.top);
}

static void moving_above_the_first_visible_row_scrolls_up(void)
{
    ListView view = list_of(10);
    move_times(&view, 1, 5);
    move_times(&view, -1, 3);
    CHECK_EQUAL(2, view.selected);
    CHECK_EQUAL(2, view.top);
}

static void wrapping_forward_from_the_last_item_returns_to_the_top(void)
{
    ListView view = list_of(10);
    move_times(&view, 1, 9);
    list_view_move(&view, 1, true, 100);
    CHECK_EQUAL(0, view.selected);
    CHECK_EQUAL(0, view.top);
}

static void moving_past_the_end_without_wrap_stays_on_the_last_item(void)
{
    ListView view = list_of(10);
    move_times(&view, 1, 12);
    CHECK_EQUAL(9, view.selected);
    CHECK_EQUAL(7, view.top);
}

static void wrapping_back_from_the_first_item_shows_the_last_page(void)
{
    ListView view = list_of(10);
    list_view_move(&view, -1, true, 100);
    CHECK_EQUAL(9, view.selected);
    CHECK_EQUAL(7, view.top);
}

static void a_short_list_never_scrolls(void)
{
    ListView view = list_of(2);
    list_view_move(&view, -1, true, 100);
    CHECK_EQUAL(1, view.selected);
    CHECK_EQUAL(0, view.top);
}

static void an_empty_list_ignores_moves(void)
{
    ListView view = list_of(0);
    list_view_move(&view, 1, true, 100);
    list_view_move(&view, -1, true, 100);
    CHECK_EQUAL(0, view.selected);
    CHECK_EQUAL(0, view.top);
}

static void the_marquee_restarts_only_when_the_selection_changes(void)
{
    ListView view = list_of(3);
    list_view_move(&view, 1, false, 500);
    CHECK_EQUAL(500, view.selection_changed_ms);
    move_times(&view, 1, 5);
    list_view_move(&view, 1, false, 900);
    CHECK_EQUAL(100, view.selection_changed_ms);
    list_view_restart_marquee(&view, 1200);
    CHECK_EQUAL(1200, view.selection_changed_ms);
}

static void selecting_an_index_scrolls_it_into_view(void)
{
    ListView view = list_of(10);
    list_view_select(&view, 8, 300);
    CHECK_EQUAL(8, view.selected);
    CHECK_EQUAL(6, view.top);
    CHECK_EQUAL(300, view.selection_changed_ms);
    list_view_select(&view, 1, 400);
    CHECK_EQUAL(1, view.top);
}

static void selecting_past_the_end_picks_the_last_item(void)
{
    ListView view = list_of(4);
    list_view_select(&view, 9, 300);
    CHECK_EQUAL(3, view.selected);
    list_view_select(&view, -2, 300);
    CHECK_EQUAL(0, view.selected);
}

static void the_selected_row_is_highlighted(void)
{
    ListView view = list_of(10);
    move_times(&view, 1, 1);
    Canvas canvas = ui_test_canvas();
    list_view_draw(&view, &canvas, theme_content_area(&TEST_THEME), 100);
    CHECK(ui_test_area_has_only(&canvas, row_rect(1), TEST_THEME.colors.selection_background));
    CHECK(ui_test_area_has_only(&canvas, row_rect(0), TEST_THEME.colors.background));
    CHECK(ui_test_area_has_only(&canvas, row_rect(2), TEST_THEME.colors.background));
}

static void the_selected_row_has_a_shine_line_and_shades_down(void)
{
    Theme theme = TEST_THEME;
    theme.colors.selection_top = 0xFFFF;
    theme.colors.selection_shine = 0x07E0;
    ListView view = list_of(10);
    view.theme = &theme;
    move_times(&view, 1, 1);
    Canvas canvas = ui_test_canvas();
    list_view_draw(&view, &canvas, theme_content_area(&theme), 100);
    Rect row = row_rect(1);
    CHECK_EQUAL(0x07E0, canvas_pixel_at(&canvas, 0, row.y));
    CHECK_EQUAL(theme.colors.selection_background,
                canvas_pixel_at(&canvas, 0, row.y + row.height - 1));
    Color below_shine = canvas_pixel_at(&canvas, 0, row.y + 1);
    CHECK(below_shine != 0x07E0 && below_shine != theme.colors.selection_background);
}

static Theme distinct_selection_theme(ThemeSelection selection)
{
    Theme theme = TEST_THEME;
    theme.colors.selection_background = 0x001F;
    theme.colors.selection_top = 0x8410;
    theme.colors.selection_shine = 0x07E0;
    theme.colors.selection_text = 0xFFE0;
    theme.colors.chevron = 0xC618;
    theme.selection = selection;
    return theme;
}

static Rect full_row(int row)
{
    Rect area = row_rect(row);
    area.width = TEST_SCREEN_WIDTH;
    return area;
}

static Canvas draw_second_row_selected(Theme *theme)
{
    ListView view = list_of(3);
    view.theme = theme;
    items.opens_at_one = true;
    move_times(&view, 1, 1);
    Canvas canvas = ui_test_canvas();
    list_view_draw(&view, &canvas, theme_content_area(theme), 100);
    return canvas;
}

static void a_flat_selection_fills_the_whole_row_in_the_selection_background(void)
{
    Theme theme = distinct_selection_theme(THEME_SELECTION_FLAT);
    theme.colors.selection_text = theme.colors.selection_background;
    theme.colors.chevron = theme.colors.selection_background;
    Canvas canvas = draw_second_row_selected(&theme);
    CHECK(ui_test_area_has_only(&canvas, full_row(1), theme.colors.selection_background));
}

static void an_outline_selection_draws_a_frame_around_the_background(void)
{
    Theme theme = distinct_selection_theme(THEME_SELECTION_OUTLINE);
    Canvas canvas = draw_second_row_selected(&theme);
    Rect row = full_row(1);
    int last = row.y + row.height - 1;
    CHECK_EQUAL(theme.colors.selection_top, canvas_pixel_at(&canvas, 6, row.y + 1));
    CHECK_EQUAL(theme.colors.selection_top, canvas_pixel_at(&canvas, 20, last - 1));
    CHECK_EQUAL(theme.colors.selection_top, canvas_pixel_at(&canvas, 6, row.y + 2));
    CHECK_EQUAL(theme.colors.background, canvas_pixel_at(&canvas, 7, row.y + 2));
    CHECK_EQUAL(theme.colors.background, canvas_pixel_at(&canvas, 5, row.y + 2));
    CHECK_EQUAL(theme.colors.background, canvas_pixel_at(&canvas, 20, row.y));
    CHECK_EQUAL(theme.colors.background, canvas_pixel_at(&canvas, 20, last));
    CHECK(!ui_test_area_has_color(&canvas, row, theme.colors.selection_background));
}

static void an_outline_selection_keeps_clear_of_the_scrollbar(void)
{
    Theme theme = distinct_selection_theme(THEME_SELECTION_OUTLINE);
    ListView view = list_of(10);
    view.theme = &theme;
    Canvas canvas = ui_test_canvas();
    list_view_draw(&view, &canvas, theme_content_area(&theme), 100);
    Rect row = row_rect(0);
    Rect beside_scrollbar = {TEST_SCREEN_WIDTH - 6, row.y, 6, row.height};
    CHECK(!ui_test_area_has_color(&canvas, beside_scrollbar, theme.colors.selection_top));
}

static unsigned beyond_the_frame_sum(Theme *theme)
{
    Canvas canvas = draw_second_row_selected(theme);
    Rect row = full_row(1);
    unsigned sum = 0;
    for (int y = row.y; y < row.y + row.height; y++) {
        for (int x = TEST_SCREEN_WIDTH - 6; x < TEST_SCREEN_WIDTH; x++) {
            sum += canvas_pixel_at(&canvas, x, y);
        }
    }
    return sum;
}

static void an_outline_selection_draws_its_label_and_chevron_in_the_frame_color(void)
{
    Theme theme = distinct_selection_theme(THEME_SELECTION_OUTLINE);
    Canvas canvas = draw_second_row_selected(&theme);
    Rect row = full_row(1);
    Rect label = {0, row.y, 6, row.height};
    CHECK(ui_test_area_has_color(&canvas, label, theme.colors.selection_top));
    CHECK(!ui_test_area_has_color(&canvas, row, theme.colors.selection_text));
    CHECK(!ui_test_area_has_color(&canvas, row, theme.colors.chevron));
    unsigned chevron = beyond_the_frame_sum(&theme);
    theme.colors.selection_top = 0xF81F;
    CHECK(beyond_the_frame_sum(&theme) != chevron);
}

static void a_gradient_selection_draws_its_label_in_the_selection_text(void)
{
    Theme theme = distinct_selection_theme(THEME_SELECTION_GRADIENT);
    Canvas canvas = draw_second_row_selected(&theme);
    CHECK(ui_test_area_has_color(&canvas, full_row(1), theme.colors.selection_text));
}

static void the_scrollbar_thumb_sits_inset_without_a_track(void)
{
    Rect content = theme_content_area(&TEST_THEME);
    ListView long_list = list_of(10);
    Canvas canvas = ui_test_canvas();
    list_view_draw(&long_list, &canvas, content, 100);
    Rect edge = {TEST_SCREEN_WIDTH - TEST_THEME.metrics.scrollbar_inset, content.y,
                 TEST_THEME.metrics.scrollbar_inset, content.height};
    CHECK(ui_test_area_has_only(&canvas, edge, TEST_THEME.colors.background));
}

static void a_row_that_opens_something_shows_a_chevron(void)
{
    ListView view = list_of(3);
    items.opens_at_one = true;
    Canvas canvas = ui_test_canvas();
    list_view_draw(&view, &canvas, theme_content_area(&TEST_THEME), 100);
    Rect right_of_row_one = {TEST_SCREEN_WIDTH - 8, row_rect(1).y, 8, TEST_ROW_HEIGHT};
    Rect right_of_row_two = {TEST_SCREEN_WIDTH - 8, row_rect(2).y, 8, TEST_ROW_HEIGHT};
    CHECK(!ui_test_area_has_only(&canvas, right_of_row_one, TEST_THEME.colors.background));
    CHECK(ui_test_area_has_only(&canvas, right_of_row_two, TEST_THEME.colors.background));
}

static void the_scrollbar_appears_only_when_items_overflow(void)
{
    Rect content = theme_content_area(&TEST_THEME);
    int thumb_x = TEST_SCREEN_WIDTH - TEST_THEME.metrics.scrollbar_inset - 1;
    Rect right_edge = {thumb_x, content.y, 1, content.height};
    ListView long_list = list_of(10);
    Canvas canvas = ui_test_canvas();
    list_view_draw(&long_list, &canvas, content, 100);
    CHECK(ui_test_area_has_color(&canvas, right_edge, TEST_THEME.colors.scrollbar_thumb));
    ListView short_list = list_of(2);
    canvas = ui_test_canvas();
    list_view_draw(&short_list, &canvas, content, 100);
    CHECK(!ui_test_area_has_color(&canvas, right_edge, TEST_THEME.colors.scrollbar_thumb));
}

static void a_detail_is_drawn_in_the_secondary_color_at_the_right(void)
{
    ListView view = list_of(2);
    items.detail_at_one = "B";
    Canvas canvas = ui_test_canvas();
    list_view_draw(&view, &canvas, theme_content_area(&TEST_THEME), 100);
    Rect right_of_row_one = {TEST_SCREEN_WIDTH / 2, row_rect(1).y, TEST_SCREEN_WIDTH / 2,
                             TEST_ROW_HEIGHT};
    CHECK(ui_test_area_has_color(&canvas, right_of_row_one, TEST_THEME.colors.text_secondary));
}

static void a_long_detail_stops_at_its_maximum_width(void)
{
    ListView view = list_of(2);
    items.detail_at_one = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";
    model.detail_max_width = TEST_SCREEN_WIDTH / 3;
    Canvas canvas = ui_test_canvas();
    list_view_draw(&view, &canvas, theme_content_area(&TEST_THEME), 100);
    Rect left_half_of_row_one = {0, row_rect(1).y, TEST_SCREEN_WIDTH / 2, TEST_ROW_HEIGHT};
    CHECK(!ui_test_area_has_color(&canvas, left_half_of_row_one, TEST_THEME.colors.text_secondary));
    Rect right_of_row_one = {TEST_SCREEN_WIDTH / 2, row_rect(1).y, TEST_SCREEN_WIDTH / 2,
                             TEST_ROW_HEIGHT};
    CHECK(ui_test_area_has_color(&canvas, right_of_row_one, TEST_THEME.colors.text_secondary));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_new_list_selects_the_first_item),
        TEST_CASE(moving_within_the_page_keeps_the_top),
        TEST_CASE(moving_past_the_last_visible_row_scrolls_down),
        TEST_CASE(moving_above_the_first_visible_row_scrolls_up),
        TEST_CASE(wrapping_forward_from_the_last_item_returns_to_the_top),
        TEST_CASE(moving_past_the_end_without_wrap_stays_on_the_last_item),
        TEST_CASE(wrapping_back_from_the_first_item_shows_the_last_page),
        TEST_CASE(a_short_list_never_scrolls),
        TEST_CASE(an_empty_list_ignores_moves),
        TEST_CASE(the_marquee_restarts_only_when_the_selection_changes),
        TEST_CASE(selecting_an_index_scrolls_it_into_view),
        TEST_CASE(selecting_past_the_end_picks_the_last_item),
        TEST_CASE(the_selected_row_is_highlighted),
        TEST_CASE(the_scrollbar_appears_only_when_items_overflow),
        TEST_CASE(the_selected_row_has_a_shine_line_and_shades_down),
        TEST_CASE(a_flat_selection_fills_the_whole_row_in_the_selection_background),
        TEST_CASE(an_outline_selection_draws_a_frame_around_the_background),
        TEST_CASE(an_outline_selection_keeps_clear_of_the_scrollbar),
        TEST_CASE(an_outline_selection_draws_its_label_and_chevron_in_the_frame_color),
        TEST_CASE(a_gradient_selection_draws_its_label_in_the_selection_text),
        TEST_CASE(the_scrollbar_thumb_sits_inset_without_a_track),
        TEST_CASE(a_row_that_opens_something_shows_a_chevron),
        TEST_CASE(a_detail_is_drawn_in_the_secondary_color_at_the_right),
        TEST_CASE(a_long_detail_stops_at_its_maximum_width),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

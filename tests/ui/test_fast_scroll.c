#include "check.h"
#include "ui/list_navigation.h"
#include "ui_test_canvas.h"
#include "ui_test_theme.h"

enum {
    ITEM_COUNT = 100,
    ROWS_PER_SECTION = 10,
    REPEAT_MS = 120
};

static int item_count(const void *context)
{
    (void)context;
    return ITEM_COUNT;
}

static const char *item_label(const void *context, int index)
{
    (void)context;
    (void)index;
    return "A";
}

static int item_section(const void *context, int index)
{
    (void)context;
    return index / ROWS_PER_SECTION;
}

static const ListModel SECTIONED = {
    .context = NULL, .count = item_count, .label = item_label, .section = item_section};
static const ListModel PLAIN = {.context = NULL, .count = item_count, .label = item_label};

static ListView view;
static uint32_t now_ms;

static void start(const ListModel *model, int selected)
{
    list_view_init(&view, model, &TEST_THEME, 0);
    list_view_select(&view, selected, 0);
    now_ms = 1000;
}

static void send(Key key, KeyGesture gesture)
{
    ListSelection selection = {.select = NULL, .hold = NULL, .context = NULL};
    list_navigation_handle(&view, (KeyEvent){key, gesture}, now_ms, selection);
}

static void press_down(void)
{
    send(KEY_VOLUME_UP, KEY_GESTURE_PRESS);
}

static void repeat_down_at(uint32_t held_ms)
{
    now_ms = 1000 + held_ms;
    send(KEY_VOLUME_UP, KEY_GESTURE_REPEAT);
}

static void press_up(void)
{
    send(KEY_VOLUME_DOWN, KEY_GESTURE_PRESS);
}

static void repeat_up_at(uint32_t held_ms)
{
    now_ms = 1000 + held_ms;
    send(KEY_VOLUME_DOWN, KEY_GESTURE_REPEAT);
}

static void holding_moves_one_row_at_a_time_at_first(void)
{
    start(&SECTIONED, 0);
    press_down();
    repeat_down_at(400);
    repeat_down_at(400 + REPEAT_MS);
    CHECK_EQUAL(3, view.selected);
}

static void holding_longer_moves_several_rows_a_step(void)
{
    start(&PLAIN, 0);
    press_down();
    repeat_down_at(LIST_FAST_SCROLL_AFTER_MS);
    CHECK_EQUAL(1 + LIST_FAST_SCROLL_ROWS, view.selected);
}

static void a_long_hold_jumps_to_the_next_section(void)
{
    start(&SECTIONED, 3);
    press_down();
    repeat_down_at(LIST_SECTION_JUMP_AFTER_MS);
    CHECK_EQUAL(10, view.selected);
    repeat_down_at(LIST_SECTION_JUMP_AFTER_MS + REPEAT_MS);
    CHECK_EQUAL(20, view.selected);
}

static void jumping_back_lands_on_the_start_of_each_section(void)
{
    start(&SECTIONED, 26);
    press_up();
    repeat_up_at(LIST_SECTION_JUMP_AFTER_MS);
    CHECK_EQUAL(20, view.selected);
    repeat_up_at(LIST_SECTION_JUMP_AFTER_MS + REPEAT_MS);
    CHECK_EQUAL(10, view.selected);
}

static void a_list_without_sections_keeps_speeding_up(void)
{
    start(&PLAIN, 0);
    press_down();
    repeat_down_at(LIST_SECTION_JUMP_AFTER_MS);
    CHECK_EQUAL(1 + LIST_FASTEST_SCROLL_ROWS, view.selected);
}

static void fast_scrolling_stops_at_the_end(void)
{
    start(&SECTIONED, 95);
    press_down();
    repeat_down_at(LIST_SECTION_JUMP_AFTER_MS);
    CHECK_EQUAL(ITEM_COUNT - 1, view.selected);
    repeat_down_at(LIST_SECTION_JUMP_AFTER_MS + REPEAT_MS);
    CHECK_EQUAL(ITEM_COUNT - 1, view.selected);
}

static void a_new_press_starts_slow_again(void)
{
    start(&PLAIN, 0);
    press_down();
    repeat_down_at(LIST_FAST_SCROLL_AFTER_MS);
    int after_fast = view.selected;
    now_ms += 500;
    press_down();
    repeat_down_at(now_ms - 1000 + 400);
    CHECK_EQUAL(after_fast + 2, view.selected);
}

static Rect centre(void)
{
    return (Rect){TEST_SCREEN_WIDTH / 2 - 1, TEST_SCREEN_HEIGHT / 2 - 1, 2, 2};
}

static bool letter_box_drawn(uint32_t at_ms)
{
    Canvas canvas = ui_test_canvas();
    list_view_draw(&view, &canvas, theme_content_area(&TEST_THEME), at_ms);
    return ui_test_area_has_color(&canvas, centre(), TEST_THEME.colors.header_background);
}

static void the_section_shows_while_scrolling_fast_and_then_fades(void)
{
    start(&SECTIONED, 0);
    CHECK(!letter_box_drawn(now_ms));
    press_down();
    repeat_down_at(LIST_FAST_SCROLL_AFTER_MS);
    CHECK(letter_box_drawn(now_ms));
    CHECK(!letter_box_drawn(now_ms + LIST_SECTION_SHOWN_MS));
}

static void a_list_without_sections_shows_no_letter(void)
{
    start(&PLAIN, 0);
    press_down();
    repeat_down_at(LIST_FAST_SCROLL_AFTER_MS);
    CHECK(!letter_box_drawn(now_ms));
}

static void section_labels_are_letters_or_a_hash(void)
{
    CHECK_EQUAL('#', list_section_label(LIST_SECTION_BEFORE_LETTERS)[0]);
    CHECK_EQUAL('A', list_section_label(LIST_SECTION_FIRST_LETTER)[0]);
    CHECK_EQUAL('Z', list_section_label(LIST_SECTION_FIRST_LETTER + 25)[0]);
    CHECK_EQUAL('#', list_section_label(LIST_SECTION_AFTER_LETTERS)[0]);
}

static void sections_come_from_the_first_letter_of_a_sort_key(void)
{
    CHECK_EQUAL(LIST_SECTION_BEFORE_LETTERS, list_section_of_key("1999"));
    CHECK_EQUAL(LIST_SECTION_FIRST_LETTER, list_section_of_key("abba"));
    CHECK_EQUAL(LIST_SECTION_FIRST_LETTER + 25, list_section_of_key("zz top"));
    CHECK_EQUAL(LIST_SECTION_AFTER_LETTERS, list_section_of_key("\xE3\x81\x82"));
    CHECK_EQUAL(LIST_SECTION_BEFORE_LETTERS, list_section_of_key(""));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(holding_moves_one_row_at_a_time_at_first),
        TEST_CASE(holding_longer_moves_several_rows_a_step),
        TEST_CASE(a_long_hold_jumps_to_the_next_section),
        TEST_CASE(jumping_back_lands_on_the_start_of_each_section),
        TEST_CASE(a_list_without_sections_keeps_speeding_up),
        TEST_CASE(fast_scrolling_stops_at_the_end),
        TEST_CASE(a_new_press_starts_slow_again),
        TEST_CASE(the_section_shows_while_scrolling_fast_and_then_fades),
        TEST_CASE(a_list_without_sections_shows_no_letter),
        TEST_CASE(section_labels_are_letters_or_a_hash),
        TEST_CASE(sections_come_from_the_first_letter_of_a_sort_key),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

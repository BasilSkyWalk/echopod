#include "check.h"
#include "ui/message_screen.h"
#include "ui_test_canvas.h"
#include "ui_test_theme.h"

static MessageScreen message;

static Screen *open_message(void)
{
    MessageConfig config = {.title = "Note", .message = "AB", .theme = &TEST_THEME};
    return message_screen_init(&message, config);
}

static ScreenAction press(Screen *screen, Key key, KeyGesture gesture)
{
    return screen->handle_key(screen->context, (KeyEvent){key, gesture}, 100);
}

static void the_message_title_is_the_screen_title(void)
{
    Screen *screen = open_message();
    CHECK(screen->title == message.config.title);
}

static void back_pops_and_a_long_back_goes_home(void)
{
    Screen *screen = open_message();
    CHECK_EQUAL(SCREEN_ACTION_POP, press(screen, KEY_BACK, KEY_GESTURE_PRESS).type);
    CHECK_EQUAL(SCREEN_ACTION_POP_TO_ROOT, press(screen, KEY_BACK, KEY_GESTURE_LONG_PRESS).type);
}

static void other_keys_do_nothing(void)
{
    Screen *screen = open_message();
    CHECK_EQUAL(SCREEN_ACTION_NONE, press(screen, KEY_PLAY, KEY_GESTURE_PRESS).type);
    CHECK_EQUAL(SCREEN_ACTION_NONE, press(screen, KEY_VOLUME_UP, KEY_GESTURE_PRESS).type);
}

static void the_message_is_drawn_below_the_header(void)
{
    Screen *screen = open_message();
    Canvas canvas = ui_test_canvas();
    screen->draw(screen->context, &canvas);
    Rect content = theme_content_area(&TEST_THEME);
    CHECK(ui_test_area_has_color(&canvas, content, TEST_THEME.colors.text_secondary));
    CHECK(ui_test_area_has_color(&canvas, theme_header_area(&TEST_THEME),
                                 TEST_THEME.colors.header_background));
}

static void a_message_too_wide_for_the_screen_wraps_onto_more_lines(void)
{
    MessageConfig config = {
        .title = "Note", .message = "AB AB AB AB AB AB AB AB AB AB AB AB", .theme = &TEST_THEME};
    Screen *screen = message_screen_init(&message, config);
    Canvas canvas = ui_test_canvas();
    screen->draw(screen->context, &canvas);
    Rect content = theme_content_area(&TEST_THEME);
    int first = -1;
    int last = -1;
    for (int y = content.y; y < content.y + content.height; y++) {
        Rect row = {content.x, y, content.width, 1};
        if (ui_test_area_has_color(&canvas, row, TEST_THEME.colors.text_secondary)) {
            first = first < 0 ? y : first;
            last = y;
        }
    }
    CHECK(first >= 0);
    CHECK(last - first >= font_line_height(TEST_THEME.fonts.body));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_message_title_is_the_screen_title),
        TEST_CASE(back_pops_and_a_long_back_goes_home),
        TEST_CASE(other_keys_do_nothing),
        TEST_CASE(the_message_is_drawn_below_the_header),
        TEST_CASE(a_message_too_wide_for_the_screen_wraps_onto_more_lines),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

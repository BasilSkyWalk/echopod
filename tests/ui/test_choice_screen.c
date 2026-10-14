#include <stddef.h>

#include "check.h"
#include "ui/choice_screen.h"
#include "ui_test_canvas.h"
#include "ui_test_theme.h"

enum {
    OPTION_COUNT = 5
};

static const char *const OPTIONS[OPTION_COUNT] = {"A", "B", "A", "B", "A"};
static int current_value;
static int chosen_value;
static ChoiceScreen choice;

static int current_option(const void *context)
{
    (void)context;
    return current_value;
}

static void choose_option(void *context, int index)
{
    (void)context;
    chosen_value = index;
}

static Screen *open_choice(int current)
{
    current_value = current;
    chosen_value = -1;
    ChoiceConfig config = {
        .title = "Pick",
        .options = OPTIONS,
        .option_count = OPTION_COUNT,
        .current = current_option,
        .choose = choose_option,
        .context = NULL,
        .theme = &TEST_THEME,
    };
    Screen *screen = choice_screen_init(&choice, config);
    screen->enter(screen->context, 0);
    return screen;
}

static ScreenAction press(Screen *screen, Key key, KeyGesture gesture)
{
    return screen->handle_key(screen->context, (KeyEvent){key, gesture}, 100);
}

static void the_choice_title_is_the_screen_title(void)
{
    Screen *screen = open_choice(0);
    CHECK(screen->title == choice.config.title);
}

static void the_current_option_starts_selected(void)
{
    open_choice(3);
    CHECK_EQUAL(3, choice.list.selected);
    CHECK_EQUAL(1, choice.list.top);
}

static void the_current_option_is_marked(void)
{
    open_choice(1);
    CHECK(choice.model.detail(choice.model.context, 1) != NULL);
    CHECK(choice.model.detail(choice.model.context, 0) == NULL);
}

static void selecting_chooses_the_option_and_leaves(void)
{
    Screen *screen = open_choice(0);
    press(screen, KEY_VOLUME_DOWN, KEY_GESTURE_PRESS);
    press(screen, KEY_VOLUME_DOWN, KEY_GESTURE_PRESS);
    ScreenAction action = press(screen, KEY_PLAY, KEY_GESTURE_PRESS);
    CHECK_EQUAL(2, chosen_value);
    CHECK_EQUAL(SCREEN_ACTION_POP, action.type);
}

static void back_leaves_without_choosing(void)
{
    Screen *screen = open_choice(2);
    CHECK_EQUAL(SCREEN_ACTION_POP, press(screen, KEY_BACK, KEY_GESTURE_PRESS).type);
    CHECK_EQUAL(-1, chosen_value);
}

static void the_list_is_drawn_under_the_header(void)
{
    Screen *screen = open_choice(0);
    Canvas canvas = ui_test_canvas();
    screen->update(screen->context, 100);
    screen->draw(screen->context, &canvas);
    Rect first_row = {0, TEST_HEADER_HEIGHT, 1, TEST_ROW_HEIGHT};
    CHECK(ui_test_area_has_only(&canvas, first_row, TEST_THEME.colors.selection_background));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_choice_title_is_the_screen_title),
        TEST_CASE(the_current_option_starts_selected),
        TEST_CASE(the_current_option_is_marked),
        TEST_CASE(selecting_chooses_the_option_and_leaves),
        TEST_CASE(back_leaves_without_choosing),
        TEST_CASE(the_list_is_drawn_under_the_header),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

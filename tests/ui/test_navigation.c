#include "check.h"
#include "ui/navigation.h"

static Navigation navigate(Key key, KeyGesture gesture)
{
    return navigation_from_key((KeyEvent){.key = key, .gesture = gesture});
}

static void volume_up_moves_to_the_previous_item(void)
{
    CHECK_EQUAL(NAVIGATION_PREVIOUS, navigate(KEY_VOLUME_UP, KEY_GESTURE_PRESS).intent);
    CHECK(!navigate(KEY_VOLUME_UP, KEY_GESTURE_PRESS).repeated);
    CHECK_EQUAL(NAVIGATION_PREVIOUS, navigate(KEY_VOLUME_UP, KEY_GESTURE_REPEAT).intent);
    CHECK(navigate(KEY_VOLUME_UP, KEY_GESTURE_REPEAT).repeated);
}

static void volume_down_moves_to_the_next_item(void)
{
    CHECK_EQUAL(NAVIGATION_NEXT, navigate(KEY_VOLUME_DOWN, KEY_GESTURE_PRESS).intent);
    CHECK_EQUAL(NAVIGATION_NEXT, navigate(KEY_VOLUME_DOWN, KEY_GESTURE_REPEAT).intent);
    CHECK(navigate(KEY_VOLUME_DOWN, KEY_GESTURE_REPEAT).repeated);
}

static void play_selects_and_a_long_play_asks_for_options(void)
{
    CHECK_EQUAL(NAVIGATION_SELECT, navigate(KEY_PLAY, KEY_GESTURE_PRESS).intent);
    CHECK_EQUAL(NAVIGATION_OPTIONS, navigate(KEY_PLAY, KEY_GESTURE_LONG_PRESS).intent);
}

static void back_goes_back_and_a_long_back_goes_home(void)
{
    CHECK_EQUAL(NAVIGATION_BACK, navigate(KEY_BACK, KEY_GESTURE_PRESS).intent);
    CHECK_EQUAL(NAVIGATION_HOME, navigate(KEY_BACK, KEY_GESTURE_LONG_PRESS).intent);
}

static void menu_and_power_do_not_navigate(void)
{
    CHECK_EQUAL(NAVIGATION_NONE, navigate(KEY_MENU, KEY_GESTURE_PRESS).intent);
    CHECK_EQUAL(NAVIGATION_NONE, navigate(KEY_POWER, KEY_GESTURE_PRESS).intent);
    CHECK_EQUAL(NAVIGATION_NONE, navigate(KEY_POWER, KEY_GESTURE_LONG_PRESS).intent);
}

static ScreenActionType exit_action(Key key, KeyGesture gesture)
{
    return navigation_exit_action((KeyEvent){.key = key, .gesture = gesture}).type;
}

static void back_and_home_leave_a_screen(void)
{
    CHECK_EQUAL(SCREEN_ACTION_POP, exit_action(KEY_BACK, KEY_GESTURE_PRESS));
    CHECK_EQUAL(SCREEN_ACTION_POP_TO_ROOT, exit_action(KEY_BACK, KEY_GESTURE_LONG_PRESS));
}

static void other_keys_stay_on_the_screen(void)
{
    CHECK_EQUAL(SCREEN_ACTION_NONE, exit_action(KEY_PLAY, KEY_GESTURE_PRESS));
    CHECK_EQUAL(SCREEN_ACTION_NONE, exit_action(KEY_VOLUME_DOWN, KEY_GESTURE_REPEAT));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(volume_up_moves_to_the_previous_item),
        TEST_CASE(volume_down_moves_to_the_next_item),
        TEST_CASE(play_selects_and_a_long_play_asks_for_options),
        TEST_CASE(back_goes_back_and_a_long_back_goes_home),
        TEST_CASE(menu_and_power_do_not_navigate),
        TEST_CASE(back_and_home_leave_a_screen),
        TEST_CASE(other_keys_stay_on_the_screen),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

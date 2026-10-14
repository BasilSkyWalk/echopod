#include "check.h"
#include "screens/now_playing_screen.h"

static NowPlayingCommand command(Key key, KeyGesture gesture)
{
    return now_playing_command_for((KeyEvent){.key = key, .gesture = gesture});
}

static void volume_keys_change_the_volume_while_held(void)
{
    CHECK_EQUAL(NOW_PLAYING_VOLUME_UP, command(KEY_VOLUME_UP, KEY_GESTURE_PRESS));
    CHECK_EQUAL(NOW_PLAYING_VOLUME_UP, command(KEY_VOLUME_UP, KEY_GESTURE_REPEAT));
    CHECK_EQUAL(NOW_PLAYING_VOLUME_DOWN, command(KEY_VOLUME_DOWN, KEY_GESTURE_PRESS));
    CHECK_EQUAL(NOW_PLAYING_VOLUME_DOWN, command(KEY_VOLUME_DOWN, KEY_GESTURE_REPEAT));
}

static void play_toggles_pause(void)
{
    CHECK_EQUAL(NOW_PLAYING_TOGGLE_PAUSE, command(KEY_PLAY, KEY_GESTURE_PRESS));
    CHECK_EQUAL(NOW_PLAYING_NONE, command(KEY_PLAY, KEY_GESTURE_LONG_PRESS));
}

static void menu_skips_forward_and_a_long_menu_skips_back(void)
{
    CHECK_EQUAL(NOW_PLAYING_NEXT, command(KEY_MENU, KEY_GESTURE_PRESS));
    CHECK_EQUAL(NOW_PLAYING_PREVIOUS, command(KEY_MENU, KEY_GESTURE_LONG_PRESS));
}

static void back_leaves_and_a_long_back_goes_home(void)
{
    CHECK_EQUAL(NOW_PLAYING_LEAVE, command(KEY_BACK, KEY_GESTURE_PRESS));
    CHECK_EQUAL(NOW_PLAYING_HOME, command(KEY_BACK, KEY_GESTURE_LONG_PRESS));
}

static void power_is_left_to_the_system(void)
{
    CHECK_EQUAL(NOW_PLAYING_NONE, command(KEY_POWER, KEY_GESTURE_PRESS));
    CHECK_EQUAL(NOW_PLAYING_NONE, command(KEY_POWER, KEY_GESTURE_LONG_PRESS));
}

static void progress_fills_in_proportion_to_the_position(void)
{
    CHECK_EQUAL(0, now_playing_progress_width(0, 200000, 300));
    CHECK_EQUAL(150, now_playing_progress_width(100000, 200000, 300));
    CHECK_EQUAL(300, now_playing_progress_width(200000, 200000, 300));
}

static void progress_stays_inside_the_bar(void)
{
    CHECK_EQUAL(300, now_playing_progress_width(250000, 200000, 300));
    CHECK_EQUAL(0, now_playing_progress_width(5000, 0, 300));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(volume_keys_change_the_volume_while_held),
        TEST_CASE(play_toggles_pause),
        TEST_CASE(menu_skips_forward_and_a_long_menu_skips_back),
        TEST_CASE(back_leaves_and_a_long_back_goes_home),
        TEST_CASE(power_is_left_to_the_system),
        TEST_CASE(progress_fills_in_proportion_to_the_position),
        TEST_CASE(progress_stays_inside_the_bar),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

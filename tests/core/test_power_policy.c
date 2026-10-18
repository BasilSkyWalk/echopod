#include "check.h"
#include "core/power_policy.h"

enum {
    START_MS = 1000,
    TIMEOUT_MS = 30000,
    SLEEP_MS = 60000,
    AUTO_OFF_MS = 300000
};

static PowerPolicy fresh_policy(void)
{
    PowerPolicy policy;
    power_policy_init(&policy, TIMEOUT_MS, START_MS);
    return policy;
}

static PowerKeyOutcome send(PowerPolicy *policy, Key key, KeyGesture gesture, uint32_t now_ms)
{
    return power_policy_key(policy, (KeyEvent){.key = key, .gesture = gesture}, now_ms);
}

static void the_display_starts_on_and_unlocked(void)
{
    PowerPolicy policy = fresh_policy();
    CHECK(policy.display_on);
}

static void the_display_turns_off_after_the_timeout(void)
{
    PowerPolicy policy = fresh_policy();
    power_policy_update(&policy, START_MS + TIMEOUT_MS - 1);
    CHECK(policy.display_on);
    power_policy_update(&policy, START_MS + TIMEOUT_MS);
    CHECK(!policy.display_on);
}

static void activity_restarts_the_timeout(void)
{
    PowerPolicy policy = fresh_policy();
    CHECK(!send(&policy, KEY_VOLUME_DOWN, KEY_GESTURE_PRESS, START_MS + 20000).consumed);
    power_policy_update(&policy, START_MS + TIMEOUT_MS + 1000);
    CHECK(policy.display_on);
}

static void a_zero_timeout_keeps_the_display_on(void)
{
    PowerPolicy policy = fresh_policy();
    power_policy_set_timeout(&policy, 0);
    power_policy_update(&policy, START_MS + 10 * TIMEOUT_MS);
    CHECK(policy.display_on);
}

static void after_a_timeout_media_keys_work_without_waking(void)
{
    PowerPolicy policy = fresh_policy();
    power_policy_update(&policy, START_MS + TIMEOUT_MS);
    PowerKeyOutcome outcome =
        send(&policy, KEY_VOLUME_UP, KEY_GESTURE_PRESS, START_MS + TIMEOUT_MS);
    CHECK(outcome.consumed);
    CHECK(outcome.background);
    CHECK(!policy.display_on);
}

static void a_power_tap_turns_the_screen_off_and_on(void)
{
    PowerPolicy policy = fresh_policy();
    CHECK(send(&policy, KEY_POWER, KEY_GESTURE_PRESS, START_MS).consumed);
    CHECK(!policy.display_on);
    CHECK(send(&policy, KEY_POWER, KEY_GESTURE_PRESS, START_MS + 10).consumed);
    CHECK(policy.display_on);
}

static void power_wakes_a_screen_that_timed_out(void)
{
    PowerPolicy policy = fresh_policy();
    power_policy_update(&policy, START_MS + TIMEOUT_MS);
    CHECK(send(&policy, KEY_POWER, KEY_GESTURE_PRESS, START_MS + TIMEOUT_MS + 10).consumed);
    CHECK(policy.display_on);
}

static void with_the_screen_off_play_and_volume_go_to_the_player(void)
{
    PowerPolicy policy = fresh_policy();
    send(&policy, KEY_POWER, KEY_GESTURE_PRESS, START_MS);
    CHECK(send(&policy, KEY_PLAY, KEY_GESTURE_PRESS, START_MS + 10).background);
    CHECK(send(&policy, KEY_VOLUME_UP, KEY_GESTURE_PRESS, START_MS + 20).background);
    CHECK(send(&policy, KEY_VOLUME_DOWN, KEY_GESTURE_LONG_PRESS, START_MS + 30).background);
    CHECK(!policy.display_on);
}

static void with_the_screen_off_menu_and_back_do_nothing(void)
{
    PowerPolicy policy = fresh_policy();
    send(&policy, KEY_POWER, KEY_GESTURE_PRESS, START_MS);
    PowerKeyOutcome menu = send(&policy, KEY_MENU, KEY_GESTURE_PRESS, START_MS + 10);
    PowerKeyOutcome back = send(&policy, KEY_BACK, KEY_GESTURE_LONG_PRESS, START_MS + 20);
    CHECK(menu.consumed);
    CHECK(!menu.background);
    CHECK(back.consumed);
    CHECK(!back.background);
    CHECK(!policy.display_on);
}

static void with_the_screen_on_keys_reach_the_screen(void)
{
    PowerPolicy policy = fresh_policy();
    PowerKeyOutcome outcome = send(&policy, KEY_PLAY, KEY_GESTURE_PRESS, START_MS + 10);
    CHECK(!outcome.consumed);
    CHECK(!outcome.background);
}

static void holding_power_asks_for_shutdown(void)
{
    PowerPolicy policy = fresh_policy();
    PowerKeyOutcome outcome = send(&policy, KEY_POWER, KEY_GESTURE_LONG_PRESS, START_MS);
    CHECK(outcome.consumed);
    CHECK(outcome.shutdown);
    send(&policy, KEY_POWER, KEY_GESTURE_PRESS, START_MS + 10);
    CHECK(send(&policy, KEY_POWER, KEY_GESTURE_LONG_PRESS, START_MS + 20).shutdown);
}

static void the_sleep_timer_fires_once_and_turns_the_display_off(void)
{
    PowerPolicy policy = fresh_policy();
    power_policy_set_timeout(&policy, 0);
    power_policy_start_sleep_timer(&policy, SLEEP_MS, START_MS);
    CHECK(!power_policy_update(&policy, START_MS + SLEEP_MS - 1));
    CHECK(power_policy_update(&policy, START_MS + SLEEP_MS));
    CHECK(!policy.display_on);
    CHECK(!policy.sleep_timer_active);
    CHECK(!power_policy_update(&policy, START_MS + 2 * SLEEP_MS));
}

static void a_cancelled_sleep_timer_never_fires(void)
{
    PowerPolicy policy = fresh_policy();
    power_policy_start_sleep_timer(&policy, SLEEP_MS, START_MS);
    power_policy_cancel_sleep_timer(&policy);
    CHECK(!power_policy_update(&policy, START_MS + SLEEP_MS));
}

static void a_wake_turns_the_display_on_and_restarts_the_timeout(void)
{
    PowerPolicy policy = fresh_policy();
    power_policy_update(&policy, START_MS + TIMEOUT_MS);
    CHECK(!policy.display_on);
    power_policy_wake(&policy, START_MS + 2 * TIMEOUT_MS);
    CHECK(policy.display_on);
    power_policy_update(&policy, START_MS + 3 * TIMEOUT_MS - 1);
    CHECK(policy.display_on);
}

static void a_wake_after_a_power_tap_turns_the_screen_on(void)
{
    PowerPolicy policy = fresh_policy();
    send(&policy, KEY_POWER, KEY_GESTURE_PRESS, START_MS + 10);
    power_policy_wake(&policy, START_MS + 20);
    CHECK(policy.display_on);
}

static void auto_off_shuts_down_after_its_time_without_activity(void)
{
    PowerPolicy policy = fresh_policy();
    power_policy_set_auto_off(&policy, AUTO_OFF_MS, START_MS);
    CHECK(!power_policy_update(&policy, START_MS + AUTO_OFF_MS - 1));
    CHECK(power_policy_update(&policy, START_MS + AUTO_OFF_MS));
}

static void staying_awake_and_keys_restart_the_auto_off_count(void)
{
    PowerPolicy policy = fresh_policy();
    power_policy_set_auto_off(&policy, AUTO_OFF_MS, START_MS);
    power_policy_stay_awake(&policy, START_MS + 200000);
    CHECK(!power_policy_update(&policy, START_MS + 200000 + AUTO_OFF_MS - 1));
    send(&policy, KEY_MENU, KEY_GESTURE_PRESS, START_MS + 400000);
    CHECK(!power_policy_update(&policy, START_MS + 400000 + AUTO_OFF_MS - 1));
    CHECK(power_policy_update(&policy, START_MS + 400000 + AUTO_OFF_MS));
}

static void auto_off_set_to_zero_never_shuts_down(void)
{
    PowerPolicy policy = fresh_policy();
    power_policy_set_auto_off(&policy, 0, START_MS);
    CHECK(!power_policy_update(&policy, START_MS + 10 * AUTO_OFF_MS));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_display_starts_on_and_unlocked),
        TEST_CASE(the_display_turns_off_after_the_timeout),
        TEST_CASE(activity_restarts_the_timeout),
        TEST_CASE(a_zero_timeout_keeps_the_display_on),
        TEST_CASE(after_a_timeout_media_keys_work_without_waking),
        TEST_CASE(a_power_tap_turns_the_screen_off_and_on),
        TEST_CASE(power_wakes_a_screen_that_timed_out),
        TEST_CASE(with_the_screen_off_play_and_volume_go_to_the_player),
        TEST_CASE(with_the_screen_off_menu_and_back_do_nothing),
        TEST_CASE(with_the_screen_on_keys_reach_the_screen),
        TEST_CASE(holding_power_asks_for_shutdown),
        TEST_CASE(the_sleep_timer_fires_once_and_turns_the_display_off),
        TEST_CASE(a_cancelled_sleep_timer_never_fires),
        TEST_CASE(a_wake_turns_the_display_on_and_restarts_the_timeout),
        TEST_CASE(a_wake_after_a_power_tap_turns_the_screen_on),
        TEST_CASE(auto_off_shuts_down_after_its_time_without_activity),
        TEST_CASE(staying_awake_and_keys_restart_the_auto_off_count),
        TEST_CASE(auto_off_set_to_zero_never_shuts_down),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

#include "check.h"
#include "core/power_policy.h"

enum {
    START_MS = 1000,
    TIMEOUT_MS = 30000,
    SLEEP_MS = 60000
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
    CHECK(!policy.locked);
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

static void a_key_wakes_the_display_without_doing_anything_else(void)
{
    PowerPolicy policy = fresh_policy();
    power_policy_update(&policy, START_MS + TIMEOUT_MS);
    CHECK(send(&policy, KEY_PLAY, KEY_GESTURE_PRESS, START_MS + TIMEOUT_MS + 10).consumed);
    CHECK(policy.display_on);
    CHECK(!send(&policy, KEY_PLAY, KEY_GESTURE_PRESS, START_MS + TIMEOUT_MS + 20).consumed);
}

static void power_locks_and_unlocks(void)
{
    PowerPolicy policy = fresh_policy();
    CHECK(send(&policy, KEY_POWER, KEY_GESTURE_PRESS, START_MS).consumed);
    CHECK(policy.locked);
    CHECK(!policy.display_on);
    CHECK(send(&policy, KEY_POWER, KEY_GESTURE_PRESS, START_MS + 10).consumed);
    CHECK(!policy.locked);
    CHECK(policy.display_on);
}

static void power_only_wakes_a_display_that_timed_out(void)
{
    PowerPolicy policy = fresh_policy();
    power_policy_update(&policy, START_MS + TIMEOUT_MS);
    CHECK(send(&policy, KEY_POWER, KEY_GESTURE_PRESS, START_MS + TIMEOUT_MS + 10).consumed);
    CHECK(policy.display_on);
    CHECK(!policy.locked);
}

static void a_locked_player_ignores_other_keys(void)
{
    PowerPolicy policy = fresh_policy();
    send(&policy, KEY_POWER, KEY_GESTURE_PRESS, START_MS);
    CHECK(send(&policy, KEY_PLAY, KEY_GESTURE_PRESS, START_MS + 10).consumed);
    CHECK(send(&policy, KEY_VOLUME_UP, KEY_GESTURE_REPEAT, START_MS + 20).consumed);
    CHECK(policy.locked);
    CHECK(!policy.display_on);
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

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_display_starts_on_and_unlocked),
        TEST_CASE(the_display_turns_off_after_the_timeout),
        TEST_CASE(activity_restarts_the_timeout),
        TEST_CASE(a_zero_timeout_keeps_the_display_on),
        TEST_CASE(a_key_wakes_the_display_without_doing_anything_else),
        TEST_CASE(power_locks_and_unlocks),
        TEST_CASE(power_only_wakes_a_display_that_timed_out),
        TEST_CASE(a_locked_player_ignores_other_keys),
        TEST_CASE(holding_power_asks_for_shutdown),
        TEST_CASE(the_sleep_timer_fires_once_and_turns_the_display_off),
        TEST_CASE(a_cancelled_sleep_timer_never_fires),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

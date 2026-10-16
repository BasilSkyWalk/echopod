#include "check.h"
#include "core/key_tracker.h"

enum {
    LONG_PRESS_MS = 600,
    REPEAT_DELAY_MS = 400,
    REPEAT_INTERVAL_MS = 100,
    NO_EVENT = -1
};

static KeyTracker make_tracker(void)
{
    KeyConfig config = {
        .long_press_ms = {[KEY_POWER] = 2000},
        .timing = {LONG_PRESS_MS, REPEAT_DELAY_MS, REPEAT_INTERVAL_MS},
        .behaviors =
            {
                [KEY_VOLUME_UP] = KEY_BEHAVIOR_REPEAT,
                [KEY_VOLUME_DOWN] = KEY_BEHAVIOR_REPEAT,
                [KEY_PLAY] = KEY_BEHAVIOR_TAP_OR_HOLD,
                [KEY_MENU] = KEY_BEHAVIOR_TAP_OR_HOLD,
                [KEY_BACK] = KEY_BEHAVIOR_TAP_OR_HOLD,
                [KEY_POWER] = KEY_BEHAVIOR_TAP_OR_HOLD,
            },
    };
    KeyTracker tracker;
    key_tracker_init(&tracker, &config);
    return tracker;
}

static int next_gesture(KeyTracker *tracker, Key expected_key)
{
    KeyEvent event;
    if (!key_tracker_next_event(tracker, &event)) {
        return NO_EVENT;
    }
    CHECK_EQUAL(expected_key, event.key);
    return (int)event.gesture;
}

static void tap_emits_press_on_release(void)
{
    KeyTracker tracker = make_tracker();
    key_tracker_key_down(&tracker, KEY_PLAY, 1000);
    key_tracker_update(&tracker, 1100);
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_PLAY));
    key_tracker_key_up(&tracker, KEY_PLAY, 1150);
    CHECK_EQUAL(KEY_GESTURE_PRESS, next_gesture(&tracker, KEY_PLAY));
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_PLAY));
}

static void holding_past_threshold_emits_one_long_press(void)
{
    KeyTracker tracker = make_tracker();
    key_tracker_key_down(&tracker, KEY_MENU, 1000);
    key_tracker_update(&tracker, 1000 + LONG_PRESS_MS);
    key_tracker_update(&tracker, 1000 + LONG_PRESS_MS + 500);
    CHECK_EQUAL(KEY_GESTURE_LONG_PRESS, next_gesture(&tracker, KEY_MENU));
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_MENU));
}

static void release_after_long_press_emits_nothing(void)
{
    KeyTracker tracker = make_tracker();
    key_tracker_key_down(&tracker, KEY_BACK, 1000);
    key_tracker_update(&tracker, 1000 + LONG_PRESS_MS);
    key_tracker_key_up(&tracker, KEY_BACK, 1000 + LONG_PRESS_MS + 50);
    CHECK_EQUAL(KEY_GESTURE_LONG_PRESS, next_gesture(&tracker, KEY_BACK));
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_BACK));
}

static void late_release_without_update_emits_long_press(void)
{
    KeyTracker tracker = make_tracker();
    key_tracker_key_down(&tracker, KEY_PLAY, 1000);
    key_tracker_key_up(&tracker, KEY_PLAY, 1000 + LONG_PRESS_MS);
    CHECK_EQUAL(KEY_GESTURE_LONG_PRESS, next_gesture(&tracker, KEY_PLAY));
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_PLAY));
}

static void repeat_key_emits_press_on_down(void)
{
    KeyTracker tracker = make_tracker();
    key_tracker_key_down(&tracker, KEY_VOLUME_UP, 1000);
    CHECK_EQUAL(KEY_GESTURE_PRESS, next_gesture(&tracker, KEY_VOLUME_UP));
    key_tracker_key_up(&tracker, KEY_VOLUME_UP, 1050);
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_VOLUME_UP));
}

static void repeat_key_repeats_after_delay_then_every_interval(void)
{
    KeyTracker tracker = make_tracker();
    key_tracker_key_down(&tracker, KEY_VOLUME_DOWN, 1000);
    CHECK_EQUAL(KEY_GESTURE_PRESS, next_gesture(&tracker, KEY_VOLUME_DOWN));
    key_tracker_update(&tracker, 1000 + REPEAT_DELAY_MS - 1);
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_VOLUME_DOWN));
    key_tracker_update(&tracker, 1000 + REPEAT_DELAY_MS);
    CHECK_EQUAL(KEY_GESTURE_REPEAT, next_gesture(&tracker, KEY_VOLUME_DOWN));
    key_tracker_update(&tracker, 1000 + REPEAT_DELAY_MS + REPEAT_INTERVAL_MS - 1);
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_VOLUME_DOWN));
    key_tracker_update(&tracker, 1000 + REPEAT_DELAY_MS + REPEAT_INTERVAL_MS);
    CHECK_EQUAL(KEY_GESTURE_REPEAT, next_gesture(&tracker, KEY_VOLUME_DOWN));
}

static void releasing_a_repeat_key_stops_repeats(void)
{
    KeyTracker tracker = make_tracker();
    key_tracker_key_down(&tracker, KEY_VOLUME_UP, 1000);
    CHECK_EQUAL(KEY_GESTURE_PRESS, next_gesture(&tracker, KEY_VOLUME_UP));
    key_tracker_key_up(&tracker, KEY_VOLUME_UP, 1100);
    key_tracker_update(&tracker, 1000 + REPEAT_DELAY_MS * 3);
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_VOLUME_UP));
}

static void a_second_down_while_held_is_ignored(void)
{
    KeyTracker tracker = make_tracker();
    key_tracker_key_down(&tracker, KEY_VOLUME_UP, 1000);
    key_tracker_key_down(&tracker, KEY_VOLUME_UP, 1010);
    CHECK_EQUAL(KEY_GESTURE_PRESS, next_gesture(&tracker, KEY_VOLUME_UP));
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_VOLUME_UP));
}

static void a_release_without_press_is_ignored(void)
{
    KeyTracker tracker = make_tracker();
    key_tracker_key_up(&tracker, KEY_PLAY, 1000);
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_PLAY));
}

static void keys_are_tracked_independently(void)
{
    KeyTracker tracker = make_tracker();
    key_tracker_key_down(&tracker, KEY_PLAY, 1000);
    key_tracker_key_down(&tracker, KEY_BACK, 1100);
    key_tracker_key_up(&tracker, KEY_BACK, 1200);
    CHECK_EQUAL(KEY_GESTURE_PRESS, next_gesture(&tracker, KEY_BACK));
    key_tracker_update(&tracker, 1000 + LONG_PRESS_MS);
    CHECK_EQUAL(KEY_GESTURE_LONG_PRESS, next_gesture(&tracker, KEY_PLAY));
}

static void a_full_queue_keeps_the_oldest_events(void)
{
    KeyTracker tracker = make_tracker();
    for (uint32_t index = 0; index < KEY_TRACKER_QUEUE_CAPACITY + 3; index++) {
        uint32_t now_ms = 1000 + index * 10;
        Key key = index < KEY_TRACKER_QUEUE_CAPACITY ? KEY_PLAY : KEY_MENU;
        key_tracker_key_down(&tracker, key, now_ms);
        key_tracker_key_up(&tracker, key, now_ms + 1);
    }
    for (uint32_t index = 0; index < KEY_TRACKER_QUEUE_CAPACITY; index++) {
        CHECK_EQUAL(KEY_GESTURE_PRESS, next_gesture(&tracker, KEY_PLAY));
    }
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_PLAY));
}

static void repeat_timing_survives_clock_wraparound(void)
{
    KeyTracker tracker = make_tracker();
    uint32_t pressed_at_ms = UINT32_MAX - 100;
    key_tracker_key_down(&tracker, KEY_VOLUME_UP, pressed_at_ms);
    CHECK_EQUAL(KEY_GESTURE_PRESS, next_gesture(&tracker, KEY_VOLUME_UP));
    key_tracker_update(&tracker, pressed_at_ms + 50);
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_VOLUME_UP));
    key_tracker_update(&tracker, pressed_at_ms + REPEAT_DELAY_MS - 1);
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_VOLUME_UP));
    key_tracker_update(&tracker, pressed_at_ms + REPEAT_DELAY_MS);
    CHECK_EQUAL(KEY_GESTURE_REPEAT, next_gesture(&tracker, KEY_VOLUME_UP));
}

static void long_press_timing_survives_clock_wraparound(void)
{
    KeyTracker tracker = make_tracker();
    uint32_t pressed_at_ms = UINT32_MAX - 100;
    key_tracker_key_down(&tracker, KEY_MENU, pressed_at_ms);
    key_tracker_update(&tracker, pressed_at_ms + 50);
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_MENU));
    key_tracker_update(&tracker, pressed_at_ms + LONG_PRESS_MS - 1);
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_MENU));
    key_tracker_update(&tracker, pressed_at_ms + LONG_PRESS_MS);
    CHECK_EQUAL(KEY_GESTURE_LONG_PRESS, next_gesture(&tracker, KEY_MENU));
}

static KeyTracker hold_repeat_tracker(void)
{
    KeyTracker tracker = make_tracker();
    key_tracker_set_behavior(&tracker, KEY_VOLUME_UP, KEY_BEHAVIOR_TAP_HOLD_REPEAT);
    return tracker;
}

static void a_hold_repeat_key_taps_on_release(void)
{
    KeyTracker tracker = hold_repeat_tracker();
    key_tracker_key_down(&tracker, KEY_VOLUME_UP, 1000);
    key_tracker_update(&tracker, 1000 + REPEAT_DELAY_MS);
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_VOLUME_UP));
    key_tracker_key_up(&tracker, KEY_VOLUME_UP, 1000 + REPEAT_DELAY_MS + 10);
    CHECK_EQUAL(KEY_GESTURE_PRESS, next_gesture(&tracker, KEY_VOLUME_UP));
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_VOLUME_UP));
}

static void a_held_hold_repeat_key_long_presses_then_repeats(void)
{
    KeyTracker tracker = hold_repeat_tracker();
    key_tracker_key_down(&tracker, KEY_VOLUME_UP, 1000);
    key_tracker_update(&tracker, 1000 + LONG_PRESS_MS);
    CHECK_EQUAL(KEY_GESTURE_LONG_PRESS, next_gesture(&tracker, KEY_VOLUME_UP));
    key_tracker_update(&tracker, 1000 + LONG_PRESS_MS + REPEAT_INTERVAL_MS - 1);
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_VOLUME_UP));
    key_tracker_update(&tracker, 1000 + LONG_PRESS_MS + REPEAT_INTERVAL_MS);
    CHECK_EQUAL(KEY_GESTURE_REPEAT, next_gesture(&tracker, KEY_VOLUME_UP));
    key_tracker_key_up(&tracker, KEY_VOLUME_UP, 1000 + LONG_PRESS_MS + REPEAT_INTERVAL_MS + 10);
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_VOLUME_UP));
}

static void changing_a_behavior_leaves_other_keys_alone(void)
{
    KeyTracker tracker = hold_repeat_tracker();
    key_tracker_key_down(&tracker, KEY_VOLUME_DOWN, 1000);
    CHECK_EQUAL(KEY_GESTURE_PRESS, next_gesture(&tracker, KEY_VOLUME_DOWN));
}

static void a_key_with_its_own_long_press_time_waits_for_it(void)
{
    KeyTracker tracker = make_tracker();
    key_tracker_key_down(&tracker, KEY_POWER, 1000);
    key_tracker_update(&tracker, 2500);
    CHECK_EQUAL(NO_EVENT, next_gesture(&tracker, KEY_POWER));
    key_tracker_update(&tracker, 3000);
    CHECK_EQUAL(KEY_GESTURE_LONG_PRESS, next_gesture(&tracker, KEY_POWER));
}

static void a_release_before_its_own_long_press_time_is_a_tap(void)
{
    KeyTracker tracker = make_tracker();
    key_tracker_key_down(&tracker, KEY_POWER, 1000);
    key_tracker_update(&tracker, 2000);
    key_tracker_key_up(&tracker, KEY_POWER, 2500);
    CHECK_EQUAL(KEY_GESTURE_PRESS, next_gesture(&tracker, KEY_POWER));
}

static void keys_without_their_own_time_use_the_shared_one(void)
{
    KeyTracker tracker = make_tracker();
    key_tracker_key_down(&tracker, KEY_PLAY, 1000);
    key_tracker_update(&tracker, 1000 + LONG_PRESS_MS);
    CHECK_EQUAL(KEY_GESTURE_LONG_PRESS, next_gesture(&tracker, KEY_PLAY));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_key_with_its_own_long_press_time_waits_for_it),
        TEST_CASE(a_release_before_its_own_long_press_time_is_a_tap),
        TEST_CASE(keys_without_their_own_time_use_the_shared_one),
        TEST_CASE(tap_emits_press_on_release),
        TEST_CASE(holding_past_threshold_emits_one_long_press),
        TEST_CASE(release_after_long_press_emits_nothing),
        TEST_CASE(late_release_without_update_emits_long_press),
        TEST_CASE(repeat_key_emits_press_on_down),
        TEST_CASE(repeat_key_repeats_after_delay_then_every_interval),
        TEST_CASE(releasing_a_repeat_key_stops_repeats),
        TEST_CASE(a_second_down_while_held_is_ignored),
        TEST_CASE(a_release_without_press_is_ignored),
        TEST_CASE(keys_are_tracked_independently),
        TEST_CASE(a_full_queue_keeps_the_oldest_events),
        TEST_CASE(repeat_timing_survives_clock_wraparound),
        TEST_CASE(long_press_timing_survives_clock_wraparound),
        TEST_CASE(a_hold_repeat_key_taps_on_release),
        TEST_CASE(a_held_hold_repeat_key_long_presses_then_repeats),
        TEST_CASE(changing_a_behavior_leaves_other_keys_alone),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

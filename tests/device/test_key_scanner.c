#include "check.h"
#include "device/key_scanner.h"

static const uint8_t PLAY = 1u << KEY_PLAY;
static const uint8_t POWER = 1u << KEY_POWER;

static void sample_twice(KeyScanner *scanner, uint8_t mask, uint32_t time_ms)
{
    key_scanner_sample(scanner, mask, time_ms);
    key_scanner_sample(scanner, mask, time_ms + 5);
}

static void check_next(KeyScanner *scanner, Key key, bool pressed, uint32_t time_ms)
{
    HalKeyTransition transition = {0};
    CHECK(key_scanner_next(scanner, &transition));
    CHECK_EQUAL(key, transition.key);
    CHECK_EQUAL(pressed, transition.pressed);
    CHECK_EQUAL(time_ms, transition.time_ms);
}

static void check_empty(KeyScanner *scanner)
{
    HalKeyTransition transition = {0};
    CHECK(!key_scanner_next(scanner, &transition));
}

static KeyScanner armed_scanner(void)
{
    KeyScanner scanner;
    key_scanner_init(&scanner);
    sample_twice(&scanner, 0, 0);
    return scanner;
}

static void a_single_sample_is_not_a_press(void)
{
    KeyScanner scanner = armed_scanner();
    key_scanner_sample(&scanner, PLAY, 100);
    check_empty(&scanner);
}

static void two_matching_samples_are_a_press_at_the_second_sample(void)
{
    KeyScanner scanner = armed_scanner();
    sample_twice(&scanner, PLAY, 100);
    check_next(&scanner, KEY_PLAY, true, 105);
    check_empty(&scanner);
}

static void a_bounce_between_samples_is_ignored(void)
{
    KeyScanner scanner = armed_scanner();
    key_scanner_sample(&scanner, PLAY, 100);
    key_scanner_sample(&scanner, 0, 105);
    key_scanner_sample(&scanner, PLAY, 110);
    check_empty(&scanner);
}

static void a_release_follows_a_press(void)
{
    KeyScanner scanner = armed_scanner();
    sample_twice(&scanner, PLAY, 100);
    sample_twice(&scanner, 0, 200);
    check_next(&scanner, KEY_PLAY, true, 105);
    check_next(&scanner, KEY_PLAY, false, 205);
}

static void a_held_key_reports_one_press(void)
{
    KeyScanner scanner = armed_scanner();
    sample_twice(&scanner, PLAY, 100);
    sample_twice(&scanner, PLAY, 200);
    check_next(&scanner, KEY_PLAY, true, 105);
    check_empty(&scanner);
}

static void switching_ladder_keys_releases_the_old_key_first(void)
{
    KeyScanner scanner = armed_scanner();
    sample_twice(&scanner, PLAY, 100);
    sample_twice(&scanner, 1u << KEY_MENU, 200);
    check_next(&scanner, KEY_PLAY, true, 105);
    check_next(&scanner, KEY_PLAY, false, 205);
    check_next(&scanner, KEY_MENU, true, 205);
}

static void power_held_from_switch_on_is_ignored_until_released(void)
{
    KeyScanner scanner;
    key_scanner_init(&scanner);
    sample_twice(&scanner, POWER, 0);
    sample_twice(&scanner, POWER, 900);
    check_empty(&scanner);
    sample_twice(&scanner, 0, 1000);
    check_empty(&scanner);
    sample_twice(&scanner, POWER, 2000);
    check_next(&scanner, KEY_POWER, true, 2005);
}

static void ladder_keys_work_while_the_switch_on_power_press_is_held(void)
{
    KeyScanner scanner;
    key_scanner_init(&scanner);
    sample_twice(&scanner, POWER | PLAY, 0);
    check_next(&scanner, KEY_PLAY, true, 5);
    check_empty(&scanner);
}

static void tap_many(KeyScanner *scanner, uint8_t held, uint32_t time_ms)
{
    for (uint32_t index = 0; index < KEY_SCANNER_QUEUE_CAPACITY; index++) {
        sample_twice(scanner, (uint8_t)(held | PLAY), time_ms + index * 20);
        sample_twice(scanner, held, time_ms + index * 20 + 10);
    }
}

static HalKeyTransition drain(KeyScanner *scanner, uint32_t *count)
{
    HalKeyTransition last = {0};
    HalKeyTransition transition = {0};
    *count = 0;
    while (key_scanner_next(scanner, &transition)) {
        last = transition;
        (*count)++;
    }
    return last;
}

static void a_busy_queue_takes_taps_as_whole_press_release_pairs(void)
{
    KeyScanner scanner = armed_scanner();
    tap_many(&scanner, 0, 100);
    uint32_t count = 0;
    HalKeyTransition transition = {0};
    while (key_scanner_next(&scanner, &transition)) {
        CHECK_EQUAL(KEY_PLAY, transition.key);
        CHECK_EQUAL(count % 2 == 0, transition.pressed);
        count++;
    }
    CHECK(count > 0);
    CHECK_EQUAL(0u, count % 2);
}

static void a_key_held_while_the_queue_fills_is_still_released(void)
{
    KeyScanner scanner = armed_scanner();
    const uint8_t volume = 1u << KEY_VOLUME_UP;
    sample_twice(&scanner, volume, 100);
    tap_many(&scanner, volume, 200);
    sample_twice(&scanner, 0, 1000);
    uint32_t count = 0;
    HalKeyTransition last = drain(&scanner, &count);
    CHECK_EQUAL(KEY_VOLUME_UP, last.key);
    CHECK(!last.pressed);
    CHECK_EQUAL(1005u, last.time_ms);
}

static void a_press_refused_by_a_busy_queue_arrives_once_there_is_room(void)
{
    KeyScanner scanner = armed_scanner();
    const uint8_t menu = 1u << KEY_MENU;
    tap_many(&scanner, 0, 100);
    sample_twice(&scanner, menu, 1000);
    uint32_t count = 0;
    CHECK_EQUAL(KEY_PLAY, drain(&scanner, &count).key);
    sample_twice(&scanner, menu, 2000);
    check_next(&scanner, KEY_MENU, true, 2000);
    check_empty(&scanner);
}

static void a_tap_refused_by_a_busy_queue_leaves_no_release_behind(void)
{
    KeyScanner scanner = armed_scanner();
    tap_many(&scanner, 0, 100);
    sample_twice(&scanner, 1u << KEY_MENU, 1000);
    sample_twice(&scanner, 0, 1100);
    uint32_t count = 0;
    CHECK_EQUAL(KEY_PLAY, drain(&scanner, &count).key);
    sample_twice(&scanner, 0, 2000);
    check_empty(&scanner);
}

static void dropping_presses_keeps_queued_releases_in_order(void)
{
    KeyScanner scanner = armed_scanner();
    const uint8_t volume = 1u << KEY_VOLUME_UP;
    sample_twice(&scanner, volume, 100);
    check_next(&scanner, KEY_VOLUME_UP, true, 105);
    sample_twice(&scanner, 0, 200);
    sample_twice(&scanner, PLAY, 300);
    sample_twice(&scanner, 0, 400);
    sample_twice(&scanner, 1u << KEY_MENU, 500);
    key_scanner_drop_presses(&scanner);
    check_next(&scanner, KEY_VOLUME_UP, false, 205);
    check_next(&scanner, KEY_PLAY, false, 405);
    check_empty(&scanner);
}

static void a_key_whose_press_was_dropped_still_reports_its_release(void)
{
    KeyScanner scanner = armed_scanner();
    sample_twice(&scanner, PLAY, 100);
    key_scanner_drop_presses(&scanner);
    check_empty(&scanner);
    sample_twice(&scanner, 0, 200);
    sample_twice(&scanner, PLAY, 300);
    check_next(&scanner, KEY_PLAY, false, 205);
    check_next(&scanner, KEY_PLAY, true, 305);
    check_empty(&scanner);
}

static void a_reading_passing_through_another_key_on_release_is_not_a_press(void)
{
    KeyScanner scanner = armed_scanner();
    const uint8_t volume = 1u << KEY_VOLUME_UP;
    sample_twice(&scanner, volume, 100);
    key_scanner_sample(&scanner, PLAY, 400);
    key_scanner_sample(&scanner, 0, 405);
    key_scanner_sample(&scanner, 0, 410);
    check_next(&scanner, KEY_VOLUME_UP, true, 105);
    check_next(&scanner, KEY_VOLUME_UP, false, 410);
    check_empty(&scanner);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_single_sample_is_not_a_press),
        TEST_CASE(two_matching_samples_are_a_press_at_the_second_sample),
        TEST_CASE(a_bounce_between_samples_is_ignored),
        TEST_CASE(a_reading_passing_through_another_key_on_release_is_not_a_press),
        TEST_CASE(a_release_follows_a_press),
        TEST_CASE(a_held_key_reports_one_press),
        TEST_CASE(switching_ladder_keys_releases_the_old_key_first),
        TEST_CASE(power_held_from_switch_on_is_ignored_until_released),
        TEST_CASE(ladder_keys_work_while_the_switch_on_power_press_is_held),
        TEST_CASE(a_busy_queue_takes_taps_as_whole_press_release_pairs),
        TEST_CASE(a_key_held_while_the_queue_fills_is_still_released),
        TEST_CASE(a_press_refused_by_a_busy_queue_arrives_once_there_is_room),
        TEST_CASE(a_tap_refused_by_a_busy_queue_leaves_no_release_behind),
        TEST_CASE(dropping_presses_keeps_queued_releases_in_order),
        TEST_CASE(a_key_whose_press_was_dropped_still_reports_its_release),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

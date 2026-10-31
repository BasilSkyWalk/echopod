#include "check.h"
#include "settings/rocker.h"

static Key oriented(Orientation orientation, Key key)
{
    return rocker_key(key, rocker_swapped_in(orientation));
}

static void other_keys_never_move(Orientation orientation)
{
    CHECK_EQUAL(KEY_PLAY, oriented(orientation, KEY_PLAY));
    CHECK_EQUAL(KEY_MENU, oriented(orientation, KEY_MENU));
    CHECK_EQUAL(KEY_BACK, oriented(orientation, KEY_BACK));
    CHECK_EQUAL(KEY_POWER, oriented(orientation, KEY_POWER));
}

static void landscape_keeps_the_rocker(void)
{
    CHECK(!rocker_swapped_in(ORIENTATION_LANDSCAPE));
    CHECK_EQUAL(KEY_VOLUME_UP, oriented(ORIENTATION_LANDSCAPE, KEY_VOLUME_UP));
    CHECK_EQUAL(KEY_VOLUME_DOWN, oriented(ORIENTATION_LANDSCAPE, KEY_VOLUME_DOWN));
    other_keys_never_move(ORIENTATION_LANDSCAPE);
}

static void portrait_swaps_the_rocker(void)
{
    CHECK(rocker_swapped_in(ORIENTATION_PORTRAIT));
    CHECK_EQUAL(KEY_VOLUME_DOWN, oriented(ORIENTATION_PORTRAIT, KEY_VOLUME_UP));
    CHECK_EQUAL(KEY_VOLUME_UP, oriented(ORIENTATION_PORTRAIT, KEY_VOLUME_DOWN));
    other_keys_never_move(ORIENTATION_PORTRAIT);
}

static void portrait_flipped_keeps_the_rocker(void)
{
    CHECK(!rocker_swapped_in(ORIENTATION_PORTRAIT_FLIPPED));
    CHECK_EQUAL(KEY_VOLUME_UP, oriented(ORIENTATION_PORTRAIT_FLIPPED, KEY_VOLUME_UP));
    CHECK_EQUAL(KEY_VOLUME_DOWN, oriented(ORIENTATION_PORTRAIT_FLIPPED, KEY_VOLUME_DOWN));
    other_keys_never_move(ORIENTATION_PORTRAIT_FLIPPED);
}

static void the_two_portraits_disagree(void)
{
    CHECK(rocker_swapped_in(ORIENTATION_PORTRAIT) !=
          rocker_swapped_in(ORIENTATION_PORTRAIT_FLIPPED));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(landscape_keeps_the_rocker),
        TEST_CASE(portrait_swaps_the_rocker),
        TEST_CASE(portrait_flipped_keeps_the_rocker),
        TEST_CASE(the_two_portraits_disagree),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

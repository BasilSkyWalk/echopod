#include "check.h"
#include "device/key_ladder.h"

static void check_band(uint16_t low, uint16_t high, Key expected)
{
    Key key = KEY_POWER;
    CHECK(key_ladder_decode(low, &key));
    CHECK_EQUAL(expected, key);
    key = KEY_POWER;
    CHECK(key_ladder_decode(high, &key));
    CHECK_EQUAL(expected, key);
}

static void check_no_key(uint16_t reading)
{
    Key key = KEY_POWER;
    CHECK(!key_ladder_decode(reading, &key));
}

static void readings_up_to_69_are_volume_down(void)
{
    check_band(0, 69, KEY_VOLUME_DOWN);
}

static void readings_from_70_to_183_are_volume_up(void)
{
    check_band(70, 183, KEY_VOLUME_UP);
}

static void readings_from_200_to_319_are_play(void)
{
    check_band(200, 319, KEY_PLAY);
}

static void readings_from_320_to_409_are_menu(void)
{
    check_band(320, 409, KEY_MENU);
}

static void readings_from_450_to_549_are_back(void)
{
    check_band(450, 549, KEY_BACK);
}

static void readings_between_bands_are_no_key(void)
{
    check_no_key(184);
    check_no_key(199);
    check_no_key(410);
    check_no_key(449);
}

static void idle_readings_are_no_key(void)
{
    check_no_key(550);
    check_no_key(1001);
    check_no_key(1023);
}

static void the_mask_of_each_ladder_reading_has_one_key_bit(void)
{
    CHECK_EQUAL(1u << KEY_PLAY, key_ladder_mask(250));
    CHECK_EQUAL(0, key_ladder_mask(1023));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(readings_up_to_69_are_volume_down),
        TEST_CASE(readings_from_70_to_183_are_volume_up),
        TEST_CASE(readings_from_200_to_319_are_play),
        TEST_CASE(readings_from_320_to_409_are_menu),
        TEST_CASE(readings_from_450_to_549_are_back),
        TEST_CASE(readings_between_bands_are_no_key),
        TEST_CASE(idle_readings_are_no_key),
        TEST_CASE(the_mask_of_each_ladder_reading_has_one_key_bit),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

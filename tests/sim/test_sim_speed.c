#include "check.h"
#include "sim_speed.h"

static HalAudioSpeed speed_of(uint8_t quarters)
{
    return (HalAudioSpeed){.quarters = quarters};
}

static void speeds_differ_only_by_their_quarters(void)
{
    CHECK(sim_speed_same(speed_of(6), speed_of(6)));
    CHECK(!sim_speed_same(speed_of(6), speed_of(8)));
}

static void a_speed_outside_the_range_is_normal(void)
{
    CHECK(sim_speed_same(speed_of(0), speed_of(4)));
    CHECK(sim_speed_same(speed_of(9), speed_of(4)));
    CHECK_EQUAL(4, sim_speed_normal(speed_of(2)).quarters);
    CHECK_EQUAL(3, sim_speed_normal(speed_of(3)).quarters);
    CHECK_EQUAL(8, sim_speed_normal(speed_of(8)).quarters);
}

static void the_virtual_clock_advances_by_the_speed(void)
{
    CHECK_EQUAL(705, sim_speed_frames(speed_of(4), 16, 44100));
    CHECK_EQUAL(1411, sim_speed_frames(speed_of(8), 16, 44100));
    CHECK_EQUAL(1058, sim_speed_frames(speed_of(6), 16, 44100));
    CHECK_EQUAL(529, sim_speed_frames(speed_of(3), 16, 44100));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(speeds_differ_only_by_their_quarters),
        TEST_CASE(a_speed_outside_the_range_is_normal),
        TEST_CASE(the_virtual_clock_advances_by_the_speed),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

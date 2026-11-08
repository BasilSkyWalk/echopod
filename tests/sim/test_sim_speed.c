#include "check.h"
#include "sim_speed.h"

static HalAudioSpeed speed_of(uint8_t quarters, bool keep_pitch)
{
    return (HalAudioSpeed){.quarters = quarters, .keep_pitch = keep_pitch};
}

static void normal_speed_ignores_the_pitch_choice(void)
{
    CHECK(sim_speed_same(speed_of(4, true), speed_of(4, false)));
    CHECK(!sim_speed_same(speed_of(6, true), speed_of(6, false)));
    CHECK(!sim_speed_same(speed_of(6, true), speed_of(8, true)));
}

static void a_speed_outside_the_range_is_normal(void)
{
    CHECK(sim_speed_same(speed_of(0, false), speed_of(4, true)));
    CHECK(sim_speed_same(speed_of(9, false), speed_of(4, true)));
    CHECK_EQUAL(4, sim_speed_normal(speed_of(2, false)).quarters);
    CHECK_EQUAL(3, sim_speed_normal(speed_of(3, false)).quarters);
    CHECK_EQUAL(8, sim_speed_normal(speed_of(8, true)).quarters);
}

static void the_virtual_clock_advances_by_the_speed(void)
{
    CHECK_EQUAL(705, sim_speed_frames(speed_of(4, true), 16, 44100));
    CHECK_EQUAL(1411, sim_speed_frames(speed_of(8, true), 16, 44100));
    CHECK_EQUAL(1058, sim_speed_frames(speed_of(6, true), 16, 44100));
    CHECK_EQUAL(529, sim_speed_frames(speed_of(3, true), 16, 44100));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(normal_speed_ignores_the_pitch_choice),
        TEST_CASE(a_speed_outside_the_range_is_normal),
        TEST_CASE(the_virtual_clock_advances_by_the_speed),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

#include "check.h"
#include "device/stream_drift.h"

static StreamDrift ready_drift(void)
{
    StreamDrift drift;
    stream_drift_init(&drift, (StreamDriftLimits){.target = 960, .window = 192, .settle = 48});
    return drift;
}

static void a_fill_inside_the_window_is_left_alone(void)
{
    StreamDrift drift = ready_drift();
    CHECK_EQUAL(0, stream_drift_adjust(&drift, 960));
    CHECK_EQUAL(0, stream_drift_adjust(&drift, 960 + 192));
    CHECK_EQUAL(0, stream_drift_adjust(&drift, 960 - 192));
}

static void a_fill_above_the_window_drops_frames(void)
{
    StreamDrift drift = ready_drift();
    CHECK_EQUAL(-1, stream_drift_adjust(&drift, 960 + 193));
}

static void a_fill_below_the_window_repeats_frames(void)
{
    StreamDrift drift = ready_drift();
    CHECK_EQUAL(1, stream_drift_adjust(&drift, 960 - 193));
}

static void a_correction_continues_until_the_fill_settles_near_the_target(void)
{
    StreamDrift drift = ready_drift();
    stream_drift_adjust(&drift, 1300);
    CHECK_EQUAL(-1, stream_drift_adjust(&drift, 1100));
    CHECK_EQUAL(-1, stream_drift_adjust(&drift, 960 + 49));
    CHECK_EQUAL(0, stream_drift_adjust(&drift, 960 + 48));
    CHECK_EQUAL(0, stream_drift_adjust(&drift, 960 + 150));
}

static void a_correction_that_overshoots_stops_instead_of_running_away(void)
{
    StreamDrift drift = ready_drift();
    stream_drift_adjust(&drift, 600);
    CHECK_EQUAL(0, stream_drift_adjust(&drift, 1100));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_fill_inside_the_window_is_left_alone),
        TEST_CASE(a_fill_above_the_window_drops_frames),
        TEST_CASE(a_fill_below_the_window_repeats_frames),
        TEST_CASE(a_correction_continues_until_the_fill_settles_near_the_target),
        TEST_CASE(a_correction_that_overshoots_stops_instead_of_running_away),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

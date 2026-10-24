#include "check.h"
#include "core/frame_stats.h"

static FrameStats stats_with(const uint32_t *durations_us, uint32_t count)
{
    FrameStats stats;
    frame_stats_reset(&stats);
    for (uint32_t index = 0; index < count; index++) {
        frame_stats_record(&stats, durations_us[index]);
    }
    return stats;
}

static void empty_stats_report_zero(void)
{
    FrameStats stats = stats_with(NULL, 0);
    CHECK_EQUAL(0, frame_stats_latest_us(&stats));
    CHECK_EQUAL(0, frame_stats_average_us(&stats));
    CHECK_EQUAL(0, frame_stats_worst_us(&stats));
    CHECK_EQUAL(0, frame_stats_achievable_fps(&stats));
}

static void latest_is_the_most_recent_duration(void)
{
    const uint32_t durations[] = {1000, 3000, 2000};
    FrameStats stats = stats_with(durations, 3);
    CHECK_EQUAL(2000, frame_stats_latest_us(&stats));
}

static void average_is_the_mean_of_recorded_durations(void)
{
    const uint32_t durations[] = {1000, 3000, 2000};
    FrameStats stats = stats_with(durations, 3);
    CHECK_EQUAL(2000, frame_stats_average_us(&stats));
}

static void worst_is_the_longest_recorded_duration(void)
{
    const uint32_t durations[] = {1000, 7000, 2000};
    FrameStats stats = stats_with(durations, 3);
    CHECK_EQUAL(7000, frame_stats_worst_us(&stats));
}

static void window_forgets_durations_older_than_its_size(void)
{
    FrameStats stats = stats_with(NULL, 0);
    for (uint32_t index = 0; index < FRAME_STATS_WINDOW; index++) {
        frame_stats_record(&stats, 9000);
    }
    for (uint32_t index = 0; index < FRAME_STATS_WINDOW; index++) {
        frame_stats_record(&stats, 3000);
    }
    CHECK_EQUAL(3000, frame_stats_average_us(&stats));
    CHECK_EQUAL(3000, frame_stats_worst_us(&stats));
}

static void achievable_fps_follows_the_average_duration(void)
{
    const uint32_t durations[] = {20000, 20000};
    FrameStats stats = stats_with(durations, 2);
    CHECK_EQUAL(50, frame_stats_achievable_fps(&stats));
}

static void reset_forgets_everything(void)
{
    const uint32_t durations[] = {5000};
    FrameStats stats = stats_with(durations, 1);
    frame_stats_reset(&stats);
    CHECK_EQUAL(0, frame_stats_average_us(&stats));
    CHECK_EQUAL(0, frame_stats_latest_us(&stats));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(empty_stats_report_zero),
        TEST_CASE(latest_is_the_most_recent_duration),
        TEST_CASE(average_is_the_mean_of_recorded_durations),
        TEST_CASE(worst_is_the_longest_recorded_duration),
        TEST_CASE(window_forgets_durations_older_than_its_size),
        TEST_CASE(achievable_fps_follows_the_average_duration),
        TEST_CASE(reset_forgets_everything),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

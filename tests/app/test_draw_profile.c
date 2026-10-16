#include "check.h"
#include "draw_profile.h"

static void parts_add_up_within_a_frame(void)
{
    draw_profile_reset();
    draw_profile_add(DRAW_PART_COVERS, 300);
    draw_profile_add(DRAW_PART_COVERS, 200);
    draw_profile_add(DRAW_PART_TEXT, 100);
    draw_profile_finish_frame();
    CHECK_EQUAL(500, draw_profile_average_us(DRAW_PART_COVERS));
    CHECK_EQUAL(100, draw_profile_average_us(DRAW_PART_TEXT));
    CHECK_EQUAL(0, draw_profile_average_us(DRAW_PART_HASH));
}

static void frames_are_averaged(void)
{
    draw_profile_reset();
    draw_profile_add(DRAW_PART_HASH, 1000);
    draw_profile_finish_frame();
    draw_profile_add(DRAW_PART_HASH, 3000);
    draw_profile_finish_frame();
    CHECK_EQUAL(2000, draw_profile_average_us(DRAW_PART_HASH));
}

static void a_finished_frame_starts_the_next_from_zero(void)
{
    draw_profile_reset();
    draw_profile_add(DRAW_PART_AUDIO, 800);
    draw_profile_finish_frame();
    draw_profile_finish_frame();
    CHECK_EQUAL(400, draw_profile_average_us(DRAW_PART_AUDIO));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(parts_add_up_within_a_frame),
        TEST_CASE(frames_are_averaged),
        TEST_CASE(a_finished_frame_starts_the_next_from_zero),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

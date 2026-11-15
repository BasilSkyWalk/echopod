#include "check.h"
#include "core/draw_profile.h"

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

static void a_full_window_keeps_its_average_while_the_next_begins(void)
{
    draw_profile_reset();
    for (int frame = 0; frame < DRAW_PROFILE_WINDOW; frame++) {
        draw_profile_add(DRAW_PART_SHAPES, 600);
        draw_profile_finish_frame();
    }
    CHECK_EQUAL(600, draw_profile_average_us(DRAW_PART_SHAPES));
    draw_profile_add(DRAW_PART_SHAPES, 200);
    draw_profile_finish_frame();
    CHECK_EQUAL(200, draw_profile_average_us(DRAW_PART_SHAPES));
}

static uint32_t fake_now_us;

static uint32_t fake_clock(void)
{
    return fake_now_us;
}

static void time_since_a_start_is_read_from_the_clock(void)
{
    draw_profile_reset();
    CHECK_EQUAL(0, draw_profile_now());
    draw_profile_use_clock(fake_clock);
    fake_now_us = 1000;
    uint32_t started = draw_profile_now();
    fake_now_us = 1750;
    draw_profile_since(DRAW_PART_BOXES, started);
    draw_profile_finish_frame();
    CHECK_EQUAL(750, draw_profile_average_us(DRAW_PART_BOXES));
    draw_profile_use_clock(NULL);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(parts_add_up_within_a_frame),
        TEST_CASE(frames_are_averaged),
        TEST_CASE(a_finished_frame_starts_the_next_from_zero),
        TEST_CASE(a_full_window_keeps_its_average_while_the_next_begins),
        TEST_CASE(time_since_a_start_is_read_from_the_clock),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

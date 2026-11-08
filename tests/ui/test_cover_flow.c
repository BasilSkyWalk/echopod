#include "check.h"
#include "ui/cover_flow.h"

enum {
    ONE = COVER_FLOW_ONE,
    FRAME_MS = 16,
    TIME_CONSTANT_MS = COVER_FLOW_TIME_CONSTANT_MS
};

static const CoverFlowLayout LAYOUT = {
    .center_x = 160,
    .center_top = 10,
    .center_size = 100,
    .side_offset = 80,
    .side_spacing = 30,
    .side_width = 40,
    .side_near_height = 80,
    .side_far_height = 60,
    .side_near_light = 240,
    .side_far_light = 140,
    .fade_start = 2 * ONE,
    .fade_end = 2 * ONE + ONE / 2,
};

static int abs_of(int value)
{
    return value < 0 ? -value : value;
}

static void the_centre_cover_is_flat_and_full_size(void)
{
    Quad quad = cover_flow_quad(&LAYOUT, 0);
    CHECK_EQUAL(110, quad.left);
    CHECK_EQUAL(100, quad.width);
    CHECK_EQUAL(10, quad.left_top);
    CHECK_EQUAL(10, quad.right_top);
    CHECK_EQUAL(100, quad.left_height);
    CHECK_EQUAL(100, quad.right_height);
}

static void a_cover_to_the_right_faces_the_centre(void)
{
    Quad quad = cover_flow_quad(&LAYOUT, ONE);
    CHECK_EQUAL(220, quad.left);
    CHECK_EQUAL(40, quad.width);
    CHECK_EQUAL(80, quad.left_height);
    CHECK_EQUAL(60, quad.right_height);
    CHECK_EQUAL(20, quad.left_top);
    CHECK_EQUAL(30, quad.right_top);
}

static void a_cover_to_the_left_is_mirrored(void)
{
    Quad quad = cover_flow_quad(&LAYOUT, -ONE);
    CHECK_EQUAL(60, quad.left);
    CHECK_EQUAL(60, quad.left_height);
    CHECK_EQUAL(80, quad.right_height);
}

static void further_covers_are_spaced_evenly(void)
{
    CHECK_EQUAL(250, cover_flow_quad(&LAYOUT, 2 * ONE).left);
    CHECK_EQUAL(280, cover_flow_quad(&LAYOUT, 3 * ONE).left);
    CHECK_EQUAL(30, cover_flow_quad(&LAYOUT, -2 * ONE).left);
}

static void a_cover_on_its_way_shrinks_smoothly(void)
{
    Quad half = cover_flow_quad(&LAYOUT, ONE / 2);
    CHECK(half.width < 100 && half.width > 40);
    CHECK(half.left_height < 100 && half.left_height > 80);
    CHECK(half.left_height > half.right_height);
}

static void the_centre_cover_is_fully_lit(void)
{
    QuadLight light = cover_flow_light(&LAYOUT, 0);
    CHECK_EQUAL(COLOR_LIGHT_FULL, light.left);
    CHECK_EQUAL(COLOR_LIGHT_FULL, light.right);
}

static void side_covers_darken_away_from_the_centre(void)
{
    QuadLight right = cover_flow_light(&LAYOUT, ONE);
    CHECK_EQUAL(240, right.left);
    CHECK_EQUAL(140, right.right);
    QuadLight left = cover_flow_light(&LAYOUT, -2 * ONE);
    CHECK_EQUAL(140, left.left);
    CHECK_EQUAL(240, left.right);
}

static void covers_past_the_last_side_slot_fade_out(void)
{
    CHECK_EQUAL(140, cover_flow_light(&LAYOUT, 2 * ONE).right);
    QuadLight fading = cover_flow_light(&LAYOUT, 2 * ONE + ONE / 4);
    CHECK(fading.right > 0 && fading.right < 140);
    CHECK(fading.left > 0 && fading.left < 240);
    QuadLight gone = cover_flow_light(&LAYOUT, -(2 * ONE + ONE / 2));
    CHECK_EQUAL(0, gone.left);
    CHECK_EQUAL(0, gone.right);
}

static void light_fades_smoothly_as_a_cover_leaves_the_centre(void)
{
    QuadLight half = cover_flow_light(&LAYOUT, ONE / 2);
    CHECK(half.left < COLOR_LIGHT_FULL && half.left > 240);
    CHECK(half.right < COLOR_LIGHT_FULL && half.right > 140);
}

static void the_position_never_overshoots_and_arrives(void)
{
    int32_t position = 0;
    int32_t target = 3 * ONE;
    int frames = 0;
    while (position != target && frames < 100) {
        int32_t next = cover_flow_approach(position, target, FRAME_MS, TIME_CONSTANT_MS);
        CHECK(next > position && next <= target);
        position = next;
        frames++;
    }
    CHECK_EQUAL(target, position);
    CHECK(frames < 40);
}

static void the_position_also_moves_back(void)
{
    int32_t next = cover_flow_approach(2 * ONE, 0, FRAME_MS, TIME_CONSTANT_MS);
    CHECK(next < 2 * ONE && next >= 0);
}

static void no_time_means_no_movement(void)
{
    CHECK_EQUAL(ONE, cover_flow_approach(ONE, 4 * ONE, 0, TIME_CONSTANT_MS));
}

static void one_long_frame_moves_as_far_as_two_short_ones(void)
{
    int32_t target = 20 * ONE;
    int32_t once = cover_flow_approach(0, target, 2 * FRAME_MS, TIME_CONSTANT_MS);
    int32_t half = cover_flow_approach(0, target, FRAME_MS, TIME_CONSTANT_MS);
    int32_t twice = cover_flow_approach(half, target, FRAME_MS, TIME_CONSTANT_MS);
    CHECK(once - twice <= ONE / 64 && twice - once <= ONE / 64);
}

static void a_slow_frame_does_not_jump_straight_to_the_target(void)
{
    int32_t target = 20 * ONE;
    int32_t moved = cover_flow_approach(0, target, TIME_CONSTANT_MS, TIME_CONSTANT_MS);
    CHECK(moved > target / 2 && moved < target * 2 / 3);
}

static int frames_to_arrive(uint32_t time_constant_ms)
{
    int32_t position = 0;
    int frames = 0;
    while (position != 3 * ONE && frames < 1000) {
        position = cover_flow_approach(position, 3 * ONE, FRAME_MS, time_constant_ms);
        frames++;
    }
    return frames;
}

static void a_longer_time_constant_moves_more_slowly(void)
{
    int normal = frames_to_arrive(TIME_CONSTANT_MS);
    CHECK(frames_to_arrive(TIME_CONSTANT_MS * 3 / 2) > normal);
    CHECK(frames_to_arrive(TIME_CONSTANT_MS * 3 / 5) < normal);
    CHECK(cover_flow_approach(0, 3 * ONE, FRAME_MS, TIME_CONSTANT_MS * 3 / 2) <
          cover_flow_approach(0, 3 * ONE, FRAME_MS, TIME_CONSTANT_MS));
}

static void further_covers_shrink_with_depth(void)
{
    CoverFlowLayout layout = LAYOUT;
    layout.side_shrink = ONE / 4;
    Quad first = cover_flow_quad(&layout, ONE);
    Quad second = cover_flow_quad(&layout, 2 * ONE);
    CHECK_EQUAL(40, first.width);
    CHECK_EQUAL(30, second.width);
    CHECK_EQUAL(60, second.left_height);
    CHECK_EQUAL(45, second.right_height);
    CHECK_EQUAL(255, second.left);
    Quad between = cover_flow_quad(&layout, ONE + ONE / 2);
    CHECK(between.width < first.width && between.width > second.width);
    Quad left = cover_flow_quad(&layout, -2 * ONE);
    CHECK_EQUAL(30, left.width);
    CHECK_EQUAL(45, left.left_height);
}

static void stacked_covers_shrink_with_depth(void)
{
    CoverStackLayout stack = {.center_x = 85,
                              .center_top = 80,
                              .center_size = 100,
                              .side_offset = 70,
                              .side_spacing = 30,
                              .side_width = 80,
                              .side_height = 40,
                              .side_shrink = ONE / 4};
    Quad first = cover_flow_stacked_quad(&stack, ONE);
    Quad second = cover_flow_stacked_quad(&stack, 2 * ONE);
    CHECK_EQUAL(80, first.width);
    CHECK_EQUAL(40, first.left_height);
    CHECK_EQUAL(60, second.width);
    CHECK_EQUAL(30, second.left_height);
    CHECK_EQUAL(130 + 100 - 15, second.left_top);
    stack.side_shrink = 0;
    CHECK_EQUAL(80, cover_flow_stacked_quad(&stack, 2 * ONE).width);
}

static void three_covers_hide_everything_past_the_first_side_slot(void)
{
    CoverFlowLayout layout = LAYOUT;
    layout.fade_start = ONE;
    layout.fade_end = ONE + ONE / 2;
    CHECK_EQUAL(140, cover_flow_light(&layout, ONE).right);
    QuadLight fading = cover_flow_light(&layout, ONE + ONE / 4);
    CHECK(fading.right > 0 && fading.right < 140);
    QuadLight gone = cover_flow_light(&layout, 2 * ONE);
    CHECK_EQUAL(0, gone.left);
    CHECK_EQUAL(0, gone.right);
}

static void covers_are_drawn_from_the_edges_in(void)
{
    int order[COVER_FLOW_MAX_VISIBLE];
    int count = cover_flow_draw_order(5 * ONE, 20, order);
    CHECK_EQUAL(COVER_FLOW_MAX_VISIBLE, count);
    CHECK_EQUAL(5, order[count - 1]);
    for (int index = 1; index < count; index++) {
        CHECK(abs_of(order[index - 1] - 5) >= abs_of(order[index] - 5));
    }
}

static void only_existing_albums_are_drawn(void)
{
    int order[COVER_FLOW_MAX_VISIBLE];
    CHECK_EQUAL(COVER_FLOW_REACH + 1, cover_flow_draw_order(0, 20, order));
    CHECK_EQUAL(2, cover_flow_draw_order(ONE, 2, order));
    CHECK_EQUAL(1, order[1]);
    CHECK_EQUAL(0, cover_flow_draw_order(0, 0, order));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_centre_cover_is_fully_lit),
        TEST_CASE(side_covers_darken_away_from_the_centre),
        TEST_CASE(light_fades_smoothly_as_a_cover_leaves_the_centre),
        TEST_CASE(covers_past_the_last_side_slot_fade_out),
        TEST_CASE(the_centre_cover_is_flat_and_full_size),
        TEST_CASE(a_cover_to_the_right_faces_the_centre),
        TEST_CASE(a_cover_to_the_left_is_mirrored),
        TEST_CASE(further_covers_are_spaced_evenly),
        TEST_CASE(a_cover_on_its_way_shrinks_smoothly),
        TEST_CASE(the_position_never_overshoots_and_arrives),
        TEST_CASE(the_position_also_moves_back),
        TEST_CASE(no_time_means_no_movement),
        TEST_CASE(one_long_frame_moves_as_far_as_two_short_ones),
        TEST_CASE(a_slow_frame_does_not_jump_straight_to_the_target),
        TEST_CASE(a_longer_time_constant_moves_more_slowly),
        TEST_CASE(further_covers_shrink_with_depth),
        TEST_CASE(stacked_covers_shrink_with_depth),
        TEST_CASE(three_covers_hide_everything_past_the_first_side_slot),
        TEST_CASE(covers_are_drawn_from_the_edges_in),
        TEST_CASE(only_existing_albums_are_drawn),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

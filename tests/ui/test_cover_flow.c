#include "check.h"
#include "ui/cover_flow.h"

enum {
    ONE = COVER_FLOW_ONE,
    FRAME_MS = 16
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
        int32_t next = cover_flow_approach(position, target, FRAME_MS);
        CHECK(next > position && next <= target);
        position = next;
        frames++;
    }
    CHECK_EQUAL(target, position);
    CHECK(frames < 40);
}

static void the_position_also_moves_back(void)
{
    int32_t next = cover_flow_approach(2 * ONE, 0, FRAME_MS);
    CHECK(next < 2 * ONE && next >= 0);
}

static void no_time_means_no_movement(void)
{
    CHECK_EQUAL(ONE, cover_flow_approach(ONE, 4 * ONE, 0));
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
        TEST_CASE(the_centre_cover_is_flat_and_full_size),
        TEST_CASE(a_cover_to_the_right_faces_the_centre),
        TEST_CASE(a_cover_to_the_left_is_mirrored),
        TEST_CASE(further_covers_are_spaced_evenly),
        TEST_CASE(a_cover_on_its_way_shrinks_smoothly),
        TEST_CASE(the_position_never_overshoots_and_arrives),
        TEST_CASE(the_position_also_moves_back),
        TEST_CASE(no_time_means_no_movement),
        TEST_CASE(covers_are_drawn_from_the_edges_in),
        TEST_CASE(only_existing_albums_are_drawn),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

#include "check.h"
#include "gfx/rect.h"

static void check_rect(Rect expected, Rect actual)
{
    CHECK_EQUAL(expected.x, actual.x);
    CHECK_EQUAL(expected.y, actual.y);
    CHECK_EQUAL(expected.width, actual.width);
    CHECK_EQUAL(expected.height, actual.height);
}

static void overlapping_rects_intersect_in_the_shared_area(void)
{
    Rect first = {0, 0, 10, 10};
    Rect second = {5, 4, 10, 10};
    check_rect((Rect){5, 4, 5, 6}, rect_intersection(first, second));
}

static void a_contained_rect_is_its_own_intersection(void)
{
    Rect outer = {0, 0, 10, 10};
    Rect inner = {2, 3, 4, 5};
    check_rect(inner, rect_intersection(outer, inner));
}

static void disjoint_rects_have_an_empty_intersection(void)
{
    Rect first = {0, 0, 4, 4};
    Rect second = {10, 10, 4, 4};
    CHECK(rect_is_empty(rect_intersection(first, second)));
}

static void rects_that_only_touch_have_an_empty_intersection(void)
{
    Rect first = {0, 0, 4, 4};
    Rect second = {4, 0, 4, 4};
    CHECK(rect_is_empty(rect_intersection(first, second)));
}

static void rects_without_area_are_empty(void)
{
    CHECK(rect_is_empty((Rect){0, 0, 0, 5}));
    CHECK(rect_is_empty((Rect){0, 0, 5, -1}));
    CHECK(!rect_is_empty((Rect){0, 0, 1, 1}));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(overlapping_rects_intersect_in_the_shared_area),
        TEST_CASE(a_contained_rect_is_its_own_intersection),
        TEST_CASE(disjoint_rects_have_an_empty_intersection),
        TEST_CASE(rects_that_only_touch_have_an_empty_intersection),
        TEST_CASE(rects_without_area_are_empty),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

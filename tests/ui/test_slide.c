#include "check.h"
#include "ui/slide.h"

enum {
    WIDTH = 320,
    START_MS = 1000
};

static Slide started(SlideDirection direction)
{
    Slide slide;
    slide_start(&slide, START_MS, direction);
    return slide;
}

static void a_new_slide_has_not_moved(void)
{
    Slide slide = started(SLIDE_FORWARD);
    CHECK_EQUAL(0, slide_progress(&slide, START_MS, WIDTH));
    CHECK(!slide_finished(&slide, START_MS));
}

static void the_slide_eases_out(void)
{
    Slide slide = started(SLIDE_FORWARD);
    int half = slide_progress(&slide, START_MS + SLIDE_DURATION_MS / 2, WIDTH);
    CHECK_EQUAL(280, half);
    int quarter = slide_progress(&slide, START_MS + SLIDE_DURATION_MS / 4, WIDTH);
    int three_quarters = slide_progress(&slide, START_MS + SLIDE_DURATION_MS * 3 / 4, WIDTH);
    CHECK(quarter > 0 && quarter < half);
    CHECK(three_quarters - half < half - quarter);
}

static void the_slide_ends_at_the_full_width(void)
{
    Slide slide = started(SLIDE_BACK);
    CHECK_EQUAL(WIDTH, slide_progress(&slide, START_MS + SLIDE_DURATION_MS, WIDTH));
    CHECK_EQUAL(WIDTH, slide_progress(&slide, START_MS + 5 * SLIDE_DURATION_MS, WIDTH));
    CHECK(slide_finished(&slide, START_MS + SLIDE_DURATION_MS));
}

static void forward_moves_the_old_screen_left_and_back_moves_it_right(void)
{
    Slide forward = started(SLIDE_FORWARD);
    Slide back = started(SLIDE_BACK);
    uint32_t half = START_MS + SLIDE_DURATION_MS / 2;
    CHECK_EQUAL(-280, slide_leaving_x(&forward, half, WIDTH));
    CHECK_EQUAL(40, slide_arriving_x(&forward, half, WIDTH));
    CHECK_EQUAL(280, slide_leaving_x(&back, half, WIDTH));
    CHECK_EQUAL(-40, slide_arriving_x(&back, half, WIDTH));
}

static void the_title_switches_halfway(void)
{
    Slide slide = started(SLIDE_FORWARD);
    CHECK(!slide_shows_arriving_title(&slide, START_MS + SLIDE_DURATION_MS / 2 - 1));
    CHECK(slide_shows_arriving_title(&slide, START_MS + SLIDE_DURATION_MS / 2));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_new_slide_has_not_moved),
        TEST_CASE(the_slide_eases_out),
        TEST_CASE(the_slide_ends_at_the_full_width),
        TEST_CASE(forward_moves_the_old_screen_left_and_back_moves_it_right),
        TEST_CASE(the_title_switches_halfway),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

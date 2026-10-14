#include "check.h"
#include "gfx/frame_diff.h"

enum {
    WIDTH = 8,
    HEIGHT = 12,
    STRIP = 4
};

static Color pixels[WIDTH * STRIP];

static void fill(Color color)
{
    for (int index = 0; index < WIDTH * STRIP; index++) {
        pixels[index] = color;
    }
}

static FrameDiff fresh(void)
{
    static uint32_t hashes[HEIGHT];
    FrameDiff diff;
    frame_diff_init(&diff, hashes, HEIGHT);
    return diff;
}

static Rect strip_at(int y)
{
    return (Rect){0, y, WIDTH, STRIP};
}

static void the_first_frame_sends_every_row(void)
{
    FrameDiff diff = fresh();
    Rect changed;
    fill(0x1234);
    CHECK(frame_diff_changed(&diff, strip_at(4), pixels, &changed));
    CHECK_EQUAL(4, changed.y);
    CHECK_EQUAL(STRIP, changed.height);
    CHECK_EQUAL(WIDTH, changed.width);
}

static void an_unchanged_strip_sends_nothing(void)
{
    FrameDiff diff = fresh();
    Rect changed;
    fill(0x1234);
    frame_diff_changed(&diff, strip_at(0), pixels, &changed);
    CHECK(!frame_diff_changed(&diff, strip_at(0), pixels, &changed));
}

static void only_the_changed_rows_are_sent(void)
{
    FrameDiff diff = fresh();
    Rect changed;
    fill(0x1234);
    frame_diff_changed(&diff, strip_at(8), pixels, &changed);
    pixels[1 * WIDTH + 3] = 0xFFFF;
    CHECK(frame_diff_changed(&diff, strip_at(8), pixels, &changed));
    CHECK_EQUAL(9, changed.y);
    CHECK_EQUAL(1, changed.height);
}

static void separate_changed_rows_are_sent_as_one_span(void)
{
    FrameDiff diff = fresh();
    Rect changed;
    fill(0x1234);
    frame_diff_changed(&diff, strip_at(0), pixels, &changed);
    pixels[0] = 0x0001;
    pixels[3 * WIDTH] = 0x0001;
    CHECK(frame_diff_changed(&diff, strip_at(0), pixels, &changed));
    CHECK_EQUAL(0, changed.y);
    CHECK_EQUAL(4, changed.height);
}

static void forgetting_sends_everything_again(void)
{
    FrameDiff diff = fresh();
    Rect changed;
    fill(0x1234);
    frame_diff_changed(&diff, strip_at(0), pixels, &changed);
    frame_diff_forget(&diff);
    CHECK(frame_diff_changed(&diff, strip_at(0), pixels, &changed));
    CHECK_EQUAL(STRIP, changed.height);
}

static void a_strip_past_the_tracked_rows_is_always_sent(void)
{
    FrameDiff diff = fresh();
    Rect changed;
    fill(0x1234);
    CHECK(frame_diff_changed(&diff, (Rect){0, HEIGHT - 2, WIDTH, STRIP}, pixels, &changed));
    CHECK_EQUAL(HEIGHT - 2, changed.y);
    CHECK_EQUAL(STRIP, changed.height);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_first_frame_sends_every_row),
        TEST_CASE(an_unchanged_strip_sends_nothing),
        TEST_CASE(only_the_changed_rows_are_sent),
        TEST_CASE(separate_changed_rows_are_sent_as_one_span),
        TEST_CASE(forgetting_sends_everything_again),
        TEST_CASE(a_strip_past_the_tracked_rows_is_always_sent),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

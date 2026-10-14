#include "check.h"
#include "gfx/strip_renderer.h"

enum {
    SCREEN_WIDTH = 6,
    SCREEN_HEIGHT = 5,
    STRIP_HEIGHT = 2,
    EXPECTED_STRIPS = 3,
    MAX_RECORDED = 8,
    BACKGROUND = 0x0042,
    PAINT = 0xBEEF
};

typedef struct Recording {
    Rect drawn_bounds[MAX_RECORDED];
    int draw_count;
    Rect written_areas[MAX_RECORDED];
    int write_count;
    Color frame[SCREEN_HEIGHT][SCREEN_WIDTH];
    Rect rect_to_paint;
} Recording;

static Color strip_pixels[SCREEN_WIDTH * STRIP_HEIGHT];

static void record_draw(void *context, Canvas *canvas)
{
    Recording *recording = context;
    recording->drawn_bounds[recording->draw_count++] = canvas->bounds;
    canvas_fill_rect(canvas, recording->rect_to_paint, PAINT);
}

static void record_write(void *context, Rect area, const Color *pixels)
{
    Recording *recording = context;
    recording->written_areas[recording->write_count++] = area;
    for (int row = 0; row < area.height; row++) {
        for (int column = 0; column < area.width; column++) {
            recording->frame[area.y + row][area.x + column] = pixels[row * area.width + column];
        }
    }
}

static Recording render_with_paint(Rect rect_to_paint)
{
    static Recording recording;
    recording = (Recording){.rect_to_paint = rect_to_paint};
    StripRenderer renderer = {
        .strip_pixels = strip_pixels,
        .screen_width = SCREEN_WIDTH,
        .screen_height = SCREEN_HEIGHT,
        .strip_height = STRIP_HEIGHT,
        .background = BACKGROUND,
    };
    FrameDrawer drawer = {.draw = record_draw, .context = &recording};
    FrameSink sink = {.write = record_write, .context = &recording};
    strip_renderer_render(&renderer, drawer, sink);
    return recording;
}

static void check_rect(Rect expected, Rect actual)
{
    CHECK_EQUAL(expected.x, actual.x);
    CHECK_EQUAL(expected.y, actual.y);
    CHECK_EQUAL(expected.width, actual.width);
    CHECK_EQUAL(expected.height, actual.height);
}

static void the_screen_is_drawn_and_written_as_consecutive_strips(void)
{
    Recording recording = render_with_paint((Rect){0, 0, 0, 0});
    const Rect expected[EXPECTED_STRIPS] = {{0, 0, 6, 2}, {0, 2, 6, 2}, {0, 4, 6, 1}};
    CHECK_EQUAL(EXPECTED_STRIPS, recording.draw_count);
    CHECK_EQUAL(EXPECTED_STRIPS, recording.write_count);
    for (int strip = 0; strip < EXPECTED_STRIPS; strip++) {
        check_rect(expected[strip], recording.drawn_bounds[strip]);
        check_rect(expected[strip], recording.written_areas[strip]);
    }
}

static void strips_start_from_the_background_color(void)
{
    Recording recording = render_with_paint((Rect){0, 0, 0, 0});
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            CHECK_EQUAL(BACKGROUND, recording.frame[y][x]);
        }
    }
}

static void a_rect_spanning_strips_appears_whole(void)
{
    Rect painted = {1, 1, 3, 3};
    Recording recording = render_with_paint(painted);
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            bool inside = x >= 1 && x < 4 && y >= 1 && y < 4;
            CHECK_EQUAL(inside ? PAINT : BACKGROUND, recording.frame[y][x]);
        }
    }
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_screen_is_drawn_and_written_as_consecutive_strips),
        TEST_CASE(strips_start_from_the_background_color),
        TEST_CASE(a_rect_spanning_strips_appears_whole),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

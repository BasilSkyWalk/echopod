#include "check.h"
#include "gfx/color.h"
#include "np_faces/face_shapes.h"
#include "np_faces_test.h"

enum {
    CENTRE_X = 77,
    CENTRE_DROP = 73,
    ARM_DX = 75,
    ARM_DY = -47,
    ARM_REST = 140,
    ARM_STEPS = 12,
    TUBE_RADIUS = 60,
    GLINT_STEPS = 18,
    GLINT_REACH = 59,
    GLINT_ROWS = 24,
    TICK_BASE = 1396,
    POSITION_MS = 139600,
    FALLBACK_LEVEL = 8,
    FULL_LEVEL = 15,
    COVER_RED = 0xF800,
    FAR_STEP = 6,
    BAR_PROBE = 5,
    BAR_BETWEEN = 105,
    BAR_TRACK = 3
};

#define WALNUT_TOP COLOR_RGB(72, 44, 24)
#define WALNUT_BOTTOM COLOR_RGB(40, 24, 16)
#define VINYL COLOR_RGB(16, 16, 24)
#define ALU COLOR_RGB(200, 200, 192)
#define ALU_DARK COLOR_RGB(104, 104, 96)
#define CREAM COLOR_RGB(248, 236, 208)
#define TAN COLOR_RGB(216, 184, 136)
#define TAN_DIM COLOR_RGB(192, 160, 120)
#define AMBER COLOR_RGB(248, 164, 40)
#define LABEL_RED COLOR_RGB(176, 36, 32)

static Color pixels[NP_TEST_PIXELS];
static Color other[NP_TEST_PIXELS];

static NpTestScene scene_of(int layout, NpTestState state)
{
    return np_test_scene(NP_FACE_TURNTABLE, NP_TEST_LAYOUTS[layout], state);
}

static Point centre_of(const NpTestScene *scene)
{
    return (Point){CENTRE_X, scene->layout.area.y + CENTRE_DROP};
}

static Color probe(const NpTestScene *scene, int dx, int dy)
{
    Point centre = centre_of(scene);
    np_test_draw(scene, pixels);
    return np_test_pixel(pixels, scene->layout, centre.x + dx, centre.y + dy);
}

static bool tube_at(const NpTestScene *scene, int steps)
{
    Point centre = centre_of(scene);
    Point pivot = {centre.x + ARM_DX, centre.y + ARM_DY};
    Point point = shape_polar(pivot, (uint8_t)(ARM_REST + steps), TUBE_RADIUS);
    np_test_draw(scene, pixels);
    return np_test_pixel(pixels, scene->layout, point.x, point.y) == ALU;
}

static int count_in(const NpTestScene *scene, Rect box, Color color)
{
    int count = 0;
    np_test_draw(scene, pixels);
    for (int y = box.y; y < box.y + box.height; y++) {
        for (int x = box.x; x < box.x + box.width; x++) {
            count += np_test_pixel(pixels, scene->layout, x, y) == color ? 1 : 0;
        }
    }
    return count;
}

static bool is_portrait(const NpTestScene *scene)
{
    return scene->layout.height > scene->layout.width;
}

static Rect text_row(const NpTestScene *scene, int top_row, int bottom_row, int height)
{
    Rect area = scene->layout.area;
    int x = is_portrait(scene) ? 10 : 170;
    int width = is_portrait(scene) ? 150 : 140;
    int y = bottom_row != 0 ? area.y + area.height + bottom_row : area.y + top_row;
    return (Rect){x, y, width, height};
}

static Rect title_row(const NpTestScene *scene)
{
    return text_row(scene, is_portrait(scene) ? 144 : 4, 0, 22);
}

static Rect album_row(const NpTestScene *scene)
{
    return text_row(scene, is_portrait(scene) ? 188 : 46, 0, 22);
}

static Rect modes_row(const NpTestScene *scene)
{
    return text_row(scene, 0, is_portrait(scene) ? -42 : -39, 16);
}

static Rect times_row(const NpTestScene *scene)
{
    return text_row(scene, 0, is_portrait(scene) ? -18 : -17, 16);
}

static Rect bar_row(const NpTestScene *scene)
{
    return text_row(scene, 0, is_portrait(scene) ? -23 : -22, 3);
}

static Color bar_pixel(const NpTestScene *scene, int offset)
{
    Rect bar = bar_row(scene);
    np_test_draw(scene, pixels);
    return np_test_pixel(pixels, scene->layout, bar.x + offset, bar.y + 1);
}

static bool rows_differ(const Color *first, const Color *second, NpTestLayout layout, int row)
{
    for (int x = 0; x < layout.width; x++) {
        if (np_test_pixel(first, layout, x, row) != np_test_pixel(second, layout, x, row)) {
            return true;
        }
    }
    return false;
}

static bool rect_differs(const Color *first, const Color *second, NpTestLayout layout, Rect box)
{
    for (int y = box.y; y < box.y + box.height; y++) {
        for (int x = box.x; x < box.x + box.width; x++) {
            if (np_test_pixel(first, layout, x, y) != np_test_pixel(second, layout, x, y)) {
                return true;
            }
        }
    }
    return false;
}

static NpTestScene plain(NpTestScene scene)
{
    scene.frame.song = "xy";
    scene.frame.artist = "xy";
    scene.frame.album = "xy";
    scene.frame.detail = "xy";
    scene.frame.modes = "xy";
    return scene;
}

static NpTestScene blank(NpTestScene scene)
{
    scene.frame.song = NULL;
    scene.frame.artist = NULL;
    scene.frame.album = NULL;
    scene.frame.detail = NULL;
    scene.frame.modes = NULL;
    scene.frame.codec = NULL;
    scene.frame.stream = NULL;
    scene.frame.lyric = NULL;
    scene.frame.feedback = NOW_PLAYING_FEEDBACK_NONE;
    scene.frame.volume_shown = false;
    scene.frame.scrubbing = false;
    return scene;
}

static bool ink_is(const NpTestScene *scene, Rect box, Color color, uint8_t level)
{
    static Color bare[NP_TEST_PIXELS];
    NpTestScene empty = blank(*scene);
    np_test_draw(&empty, bare);
    np_test_draw(scene, pixels);
    int ink = 0;
    for (int y = box.y; y < box.y + box.height; y++) {
        for (int x = box.x; x < box.x + box.width; x++) {
            Color before = np_test_pixel(bare, scene->layout, x, y);
            Color after = np_test_pixel(pixels, scene->layout, x, y);
            if (after != before && after != color_blend(before, color, level)) {
                return false;
            }
            ink += after != before ? 1 : 0;
        }
    }
    return ink > 0;
}

static bool ink_free(const NpTestScene *scene, Rect box)
{
    static Color bare[NP_TEST_PIXELS];
    NpTestScene empty = blank(*scene);
    np_test_draw(&empty, bare);
    np_test_draw(scene, pixels);
    return !rect_differs(pixels, bare, scene->layout, box);
}

static void the_backdrop_runs_from_walnut_top_to_walnut_bottom(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        Rect area = scene.layout.area;
        np_test_draw(&scene, pixels);
        CHECK_EQUAL(WALNUT_TOP, np_test_pixel(pixels, scene.layout, area.x, area.y));
        CHECK_EQUAL(WALNUT_BOTTOM,
                    np_test_pixel(pixels, scene.layout, area.x, area.y + area.height - 1));
    }
}

static void a_cover_fills_the_record_as_a_picture_disc(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        scene.frame.tick = GLINT_STEPS / 4;
        CHECK_EQUAL(COVER_RED, probe(&scene, 10, 10));
        CHECK_EQUAL(COVER_RED, probe(&scene, -42, -42));
        CHECK_EQUAL(COVER_RED, probe(&scene, -62, 0));
        CHECK_EQUAL(COVER_RED, probe(&scene, 0, -62));
        CHECK_EQUAL(VINYL, probe(&scene, 4, 0));
        CHECK_EQUAL(ALU, probe(&scene, 0, 0));
        CHECK_EQUAL(ALU, probe(&scene, 0, -65));
        CHECK(probe(&scene, -50, -50) != COVER_RED);
        CHECK(probe(&scene, -60, 40) != COVER_RED);
    }
}

static void a_picture_disc_has_no_grooves_but_the_glint_still_turns(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        scene.frame.tick = GLINT_STEPS / 4;
        CHECK_EQUAL(COVER_RED, probe(&scene, -32, -32));
        CHECK_EQUAL(COVER_RED, probe(&scene, -37, -37));
        CHECK_EQUAL(COVER_RED, probe(&scene, -42, -42));
        scene.frame.tick = 0;
        CHECK_EQUAL(ALU, probe(&scene, 0, -50));
        scene.frame.tick = GLINT_STEPS / 2;
        CHECK_EQUAL(COVER_RED, probe(&scene, 0, -50));
    }
}

static void without_a_cover_the_label_is_red_with_a_cream_ring(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_NO_COVER);
        CHECK_EQUAL(LABEL_RED, probe(&scene, 10, 10));
        CHECK_EQUAL(CREAM, probe(&scene, 16, 16));
        CHECK_EQUAL(VINYL, probe(&scene, 27, 27));
    }
}

static void without_a_cover_the_rim_and_three_grooves_ring_the_record(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_NO_COVER);
        CHECK_EQUAL(ALU, probe(&scene, 0, -65));
        CHECK(probe(&scene, 32, 32) != VINYL);
        CHECK(probe(&scene, 37, 37) != VINYL);
        CHECK(probe(&scene, 42, 42) != VINYL);
        CHECK_EQUAL(VINYL, probe(&scene, 34, 34));
    }
}

static void the_glint_makes_one_turn_in_eighteen_ticks(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_NO_COVER);
        scene.frame.tick = 0;
        CHECK_EQUAL(ALU, probe(&scene, 0, -50));
        np_test_draw(&scene, other);
        scene.frame.tick = GLINT_STEPS / 2;
        CHECK_EQUAL(ALU, probe(&scene, 0, 50));
        CHECK_EQUAL(VINYL, probe(&scene, 0, -50));
        scene.frame.tick = GLINT_STEPS;
        np_test_draw(&scene, pixels);
        CHECK(np_test_same(pixels, other, scene.layout));
    }
}

static void the_glint_turns_clockwise_from_the_top(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_NO_COVER);
        scene.frame.tick = 5;
        CHECK_EQUAL(VINYL, probe(&scene, -49, 9));
        scene.frame.tick = GLINT_STEPS - 5;
        CHECK_EQUAL(ALU, probe(&scene, -49, 9));
    }
}

static void one_tick_repaints_only_the_glint_band(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        for (uint32_t tick = TICK_BASE; tick < TICK_BASE + GLINT_STEPS; tick++) {
            NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
            scene.frame.tick = tick;
            np_test_draw(&scene, other);
            scene.frame.tick = tick + 1;
            np_test_draw(&scene, pixels);
            int rows = 0;
            for (int y = 0; y < scene.layout.height; y++) {
                bool differs = rows_differ(pixels, other, scene.layout, y);
                rows += differs ? 1 : 0;
                CHECK(!differs || (y >= centre_of(&scene).y - GLINT_REACH &&
                                   y <= centre_of(&scene).y + GLINT_REACH));
            }
            CHECK(rows > 0 && rows <= GLINT_ROWS);
        }
    }
}

static void the_lamp_is_amber_when_playing_and_dark_when_paused(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene playing = scene_of(layout, NP_TEST_PLAYING);
        NpTestScene paused = scene_of(layout, NP_TEST_PAUSED);
        CHECK_EQUAL(AMBER, probe(&playing, 80, 59));
        CHECK_EQUAL(ALU_DARK, probe(&paused, 80, 59));
    }
}

static void a_playing_face_with_fitting_text_ignores_the_clock(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        np_test_draw(&scene, other);
        scene.frame.now_ms += NP_FACE_TICK_MS * 9;
        np_test_draw(&scene, pixels);
        CHECK(np_test_same(pixels, other, scene.layout));
    }
}

static void the_stylus_sits_at_the_start_with_nothing_played_and_unknown_duration(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        scene.frame.shown_ms = 0;
        CHECK(tube_at(&scene, 0));
        CHECK(!tube_at(&scene, ARM_STEPS));
        scene.frame.shown_ms = 100000;
        scene.status.duration_ms = 0;
        CHECK(tube_at(&scene, 0));
        CHECK(!tube_at(&scene, ARM_STEPS));
    }
}

static void the_stylus_reaches_the_last_groove_at_the_end(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        scene.frame.shown_ms = scene.status.duration_ms;
        CHECK(tube_at(&scene, ARM_STEPS));
        CHECK(!tube_at(&scene, 0));
        scene.frame.shown_ms = scene.status.duration_ms * 2;
        CHECK(tube_at(&scene, ARM_STEPS));
    }
}

static void the_arm_advances_monotonically_with_progress(void)
{
    NpTestScene scene = scene_of(0, NP_TEST_PLAYING);
    for (int step = 0; step <= ARM_STEPS; step++) {
        scene.frame.shown_ms =
            ((uint32_t)step * scene.status.duration_ms + ARM_STEPS - 1) / ARM_STEPS;
        int far = step < FAR_STEP ? step + FAR_STEP : step - FAR_STEP;
        CHECK(tube_at(&scene, step));
        CHECK(!tube_at(&scene, far));
    }
}

static void scrubbing_swings_the_arm_to_the_target_but_the_glint_keeps_the_tick(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_SCRUBBING);
        CHECK(tube_at(&scene, 9));
        CHECK(!tube_at(&scene, 6));
        scene.frame.tick = 0;
        CHECK_EQUAL(ALU, probe(&scene, 0, -50));
    }
}

static void volume_feedback_leaves_the_arm_where_playback_is(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_VOLUME);
        CHECK(tube_at(&scene, 6));
        CHECK(!tube_at(&scene, 9));
    }
}

static void the_bar_is_cream_when_playing_amber_when_scrubbing_and_track_past_the_fill(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene playing = scene_of(layout, NP_TEST_PLAYING);
        NpTestScene scrubbing = scene_of(layout, NP_TEST_SCRUBBING);
        CHECK_EQUAL(CREAM, bar_pixel(&playing, BAR_PROBE));
        CHECK_EQUAL(ALU_DARK, bar_pixel(&playing, BAR_BETWEEN));
        CHECK_EQUAL(AMBER, bar_pixel(&scrubbing, BAR_PROBE));
        CHECK_EQUAL(AMBER, bar_pixel(&scrubbing, BAR_BETWEEN));
        CHECK_EQUAL(ALU_DARK, bar_pixel(&scrubbing, bar_row(&scrubbing).width - BAR_TRACK));
    }
}

static void the_volume_fills_the_bar_in_amber(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_VOLUME);
        CHECK_EQUAL(AMBER, bar_pixel(&scene, BAR_PROBE));
        CHECK_EQUAL(ALU_DARK, bar_pixel(&scene, BAR_BETWEEN));
    }
}

static void an_unknown_duration_leaves_the_bar_empty(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        scene.status.duration_ms = 0;
        CHECK_EQUAL(ALU_DARK, bar_pixel(&scene, BAR_PROBE));
    }
}

static void the_times_follow_the_scrub_target(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        Rect row = times_row(&scene);
        np_test_draw(&scene, other);
        scene.frame.shown_ms = 600000;
        np_test_draw(&scene, pixels);
        CHECK(rect_differs(pixels, other, scene.layout, row));
        scene.frame.shown_ms = POSITION_MS;
        np_test_draw(&scene, pixels);
        CHECK(!rect_differs(pixels, other, scene.layout, row));
    }
}

static void the_modes_row_shows_the_modes_then_the_hint_and_the_volume_in_cream(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene playing = plain(scene_of(layout, NP_TEST_PLAYING));
        NpTestScene scrubbing = plain(scene_of(layout, NP_TEST_SCRUBBING));
        NpTestScene volume = plain(scene_of(layout, NP_TEST_VOLUME));
        Rect row = modes_row(&playing);
        CHECK(ink_is(&playing, row, TAN_DIM, FALLBACK_LEVEL));
        CHECK(ink_is(&scrubbing, row, CREAM, FALLBACK_LEVEL));
        CHECK(ink_is(&volume, row, CREAM, FALLBACK_LEVEL));
    }
}

static void the_hint_and_the_volume_replace_the_modes_with_different_text(void)
{
    NpTestScene scrubbing = plain(scene_of(0, NP_TEST_SCRUBBING));
    NpTestScene volume = plain(scene_of(0, NP_TEST_VOLUME));
    np_test_draw(&scrubbing, other);
    np_test_draw(&volume, pixels);
    CHECK(rect_differs(pixels, other, scrubbing.layout, modes_row(&scrubbing)));
}

static void a_lyric_line_replaces_the_album_in_cream(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene album = plain(scene_of(layout, NP_TEST_PLAYING));
        NpTestScene lyric = plain(scene_of(layout, NP_TEST_LYRIC));
        Rect row = album_row(&album);
        lyric.frame.lyric = "A";
        CHECK(ink_is(&album, row, TAN_DIM, FALLBACK_LEVEL));
        CHECK(ink_is(&lyric, row, CREAM, FULL_LEVEL));
    }
}

static void the_landscape_detail_row_shows_the_detail_text(void)
{
    NpTestScene landscape = plain(scene_of(0, NP_TEST_PLAYING));
    Rect row = text_row(&landscape, 92, 0, 14);
    CHECK(ink_is(&landscape, row, TAN_DIM, FALLBACK_LEVEL));
    landscape.frame.detail = NULL;
    CHECK(ink_free(&landscape, row));
}

static void an_empty_title_draws_no_title_pixels(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        Rect row = title_row(&scene);
        scene = plain(scene);
        CHECK(ink_is(&scene, row, CREAM, FALLBACK_LEVEL));
        scene.frame.song = "";
        CHECK(ink_free(&scene, row));
    }
}

static void a_badge_puts_a_vinyl_disc_and_a_cream_icon_over_the_label(void)
{
    static const NowPlayingFeedback KINDS[] = {
        NOW_PLAYING_FEEDBACK_NEXT, NOW_PLAYING_FEEDBACK_PREVIOUS, NOW_PLAYING_FEEDBACK_PAUSE,
        NOW_PLAYING_FEEDBACK_PLAY};
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        Point centre = centre_of(&scene);
        Rect icon = {centre.x - 9, centre.y - 9, 18, 18};
        CHECK_EQUAL(COVER_RED, probe(&scene, 15, 0));
        CHECK_EQUAL(0, count_in(&scene, icon, CREAM));
        for (size_t kind = 0; kind < sizeof KINDS / sizeof KINDS[0]; kind++) {
            scene.frame.feedback = KINDS[kind];
            CHECK_EQUAL(VINYL, probe(&scene, 15, 0));
            CHECK(count_in(&scene, icon, CREAM) > 0);
        }
    }
}

static void each_badge_kind_draws_a_different_icon(void)
{
    static const NowPlayingFeedback KINDS[] = {
        NOW_PLAYING_FEEDBACK_NEXT, NOW_PLAYING_FEEDBACK_PREVIOUS, NOW_PLAYING_FEEDBACK_PAUSE,
        NOW_PLAYING_FEEDBACK_PLAY};
    NpTestScene scene = scene_of(0, NP_TEST_PLAYING);
    for (size_t first = 0; first < sizeof KINDS / sizeof KINDS[0]; first++) {
        for (size_t second = first + 1; second < sizeof KINDS / sizeof KINDS[0]; second++) {
            scene.frame.feedback = KINDS[first];
            np_test_draw(&scene, other);
            scene.frame.feedback = KINDS[second];
            np_test_draw(&scene, pixels);
            CHECK(!np_test_same(pixels, other, scene.layout));
        }
    }
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_backdrop_runs_from_walnut_top_to_walnut_bottom),
        TEST_CASE(a_cover_fills_the_record_as_a_picture_disc),
        TEST_CASE(a_picture_disc_has_no_grooves_but_the_glint_still_turns),
        TEST_CASE(without_a_cover_the_label_is_red_with_a_cream_ring),
        TEST_CASE(without_a_cover_the_rim_and_three_grooves_ring_the_record),
        TEST_CASE(the_glint_makes_one_turn_in_eighteen_ticks),
        TEST_CASE(the_glint_turns_clockwise_from_the_top),
        TEST_CASE(one_tick_repaints_only_the_glint_band),
        TEST_CASE(the_lamp_is_amber_when_playing_and_dark_when_paused),
        TEST_CASE(a_playing_face_with_fitting_text_ignores_the_clock),
        TEST_CASE(the_stylus_sits_at_the_start_with_nothing_played_and_unknown_duration),
        TEST_CASE(the_stylus_reaches_the_last_groove_at_the_end),
        TEST_CASE(the_arm_advances_monotonically_with_progress),
        TEST_CASE(scrubbing_swings_the_arm_to_the_target_but_the_glint_keeps_the_tick),
        TEST_CASE(volume_feedback_leaves_the_arm_where_playback_is),
        TEST_CASE(the_bar_is_cream_when_playing_amber_when_scrubbing_and_track_past_the_fill),
        TEST_CASE(the_volume_fills_the_bar_in_amber),
        TEST_CASE(an_unknown_duration_leaves_the_bar_empty),
        TEST_CASE(the_times_follow_the_scrub_target),
        TEST_CASE(the_modes_row_shows_the_modes_then_the_hint_and_the_volume_in_cream),
        TEST_CASE(the_hint_and_the_volume_replace_the_modes_with_different_text),
        TEST_CASE(a_lyric_line_replaces_the_album_in_cream),
        TEST_CASE(the_landscape_detail_row_shows_the_detail_text),
        TEST_CASE(an_empty_title_draws_no_title_pixels),
        TEST_CASE(a_badge_puts_a_vinyl_disc_and_a_cream_icon_over_the_label),
        TEST_CASE(each_badge_kind_draws_a_different_icon),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

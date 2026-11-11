#include "check.h"
#include "np_faces_test.h"

static Color first[NP_TEST_PIXELS];
static Color second[NP_TEST_PIXELS];

static bool paints_every_area_pixel(const NpTestScene *scene)
{
    np_test_draw(scene, first);
    return np_test_area_painted(first, scene->layout);
}

static bool writes_nothing_outside_the_area(const NpTestScene *scene)
{
    np_test_draw(scene, first);
    return np_test_outside_untouched(first, scene->layout);
}

static bool strips_match_a_full_draw(const NpTestScene *scene)
{
    np_test_draw(scene, first);
    np_test_draw_strips(scene, second);
    return np_test_same(first, second, scene->layout);
}

static bool paused_frames_ignore_the_clock(const NpTestScene *scene)
{
    if (scene->frame.playing) {
        return true;
    }
    NpTestScene later = *scene;
    later.frame.now_ms += NP_TEST_PAUSED_SKEW_MS;
    np_test_draw(scene, first);
    np_test_draw(&later, second);
    return np_test_same(first, second, scene->layout);
}

static bool every_badge_kind_keeps_the_invariants(const NpTestScene *scene)
{
    static const NowPlayingFeedback KINDS[] = {
        NOW_PLAYING_FEEDBACK_NEXT, NOW_PLAYING_FEEDBACK_PREVIOUS, NOW_PLAYING_FEEDBACK_PAUSE,
        NOW_PLAYING_FEEDBACK_PLAY};
    for (size_t kind = 0; kind < sizeof KINDS / sizeof KINDS[0]; kind++) {
        NpTestScene badge = *scene;
        badge.frame.feedback = KINDS[kind];
        if (!paints_every_area_pixel(&badge) || !writes_nothing_outside_the_area(&badge) ||
            !strips_match_a_full_draw(&badge)) {
            return false;
        }
    }
    return true;
}

static void the_faces_carry_their_magic_and_a_draw(void)
{
    CHECK_EQUAL(NP_FACES_MAGIC, NOW_PLAYING_FACES.magic);
    CHECK(NOW_PLAYING_FACES.draw != NULL);
}

static void every_face_has_chrome_and_standard_has_none(void)
{
    CHECK(NOW_PLAYING_FACES.chrome[NP_FACE_STANDARD] == NULL);
    for (int face = NP_FACE_CASSETTE; face < NP_FACE_COUNT; face++) {
        CHECK(NOW_PLAYING_FACES.chrome[face] != NULL);
    }
}

static void every_face_paints_every_pixel_of_its_area(void)
{
    CHECK(np_test_for_each_scene(paints_every_area_pixel));
}

static void no_face_writes_outside_its_area(void)
{
    CHECK(np_test_for_each_scene(writes_nothing_outside_the_area));
}

static void strip_by_strip_drawing_matches_one_full_draw(void)
{
    CHECK(np_test_for_each_scene(strips_match_a_full_draw));
}

static void a_paused_face_does_not_change_with_the_clock(void)
{
    CHECK(np_test_for_each_scene(paused_frames_ignore_the_clock));
}

static void every_badge_kind_paints_cleanly(void)
{
    CHECK(np_test_for_each_scene(every_badge_kind_keeps_the_invariants));
}

static void a_face_outside_the_clip_writes_nothing(void)
{
    NpTestScene scene = np_test_scene(NP_FACE_CASSETTE, NP_TEST_LAYOUTS[0], NP_TEST_PLAYING);
    np_test_clear(first);
    Canvas canvas = canvas_make(first, (Rect){0, 0, scene.layout.width, scene.layout.height});
    canvas.clip = (Rect){0, 0, scene.layout.width, scene.layout.area.y};
    NowPlayingFrame frame = np_test_frame(&scene);
    NOW_PLAYING_FACES.draw(&frame, &canvas);
    for (int index = 0; index < scene.layout.width * scene.layout.height; index++) {
        CHECK_EQUAL(NP_TEST_SENTINEL, first[index]);
    }
}

static void the_standard_face_draws_nothing(void)
{
    NpTestScene scene = np_test_scene(NP_FACE_STANDARD, NP_TEST_LAYOUTS[0], NP_TEST_PLAYING);
    np_test_draw(&scene, first);
    CHECK_EQUAL(NP_TEST_SENTINEL, np_test_pixel(first, scene.layout, 0, scene.layout.area.y));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_faces_carry_their_magic_and_a_draw),
        TEST_CASE(every_face_has_chrome_and_standard_has_none),
        TEST_CASE(every_face_paints_every_pixel_of_its_area),
        TEST_CASE(no_face_writes_outside_its_area),
        TEST_CASE(strip_by_strip_drawing_matches_one_full_draw),
        TEST_CASE(a_paused_face_does_not_change_with_the_clock),
        TEST_CASE(every_badge_kind_paints_cleanly),
        TEST_CASE(a_face_outside_the_clip_writes_nothing),
        TEST_CASE(the_standard_face_draws_nothing),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

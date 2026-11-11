#include <stdint.h>
#include <string.h>

#include "check.h"
#include "core/i18n.h"
#include "np_faces/face_shapes.h"
#include "np_faces_test.h"
#include "tiny_font.h"
#include "ui/label.h"

#define PLATE COLOR_RGB(32, 28, 24)
#define BEZEL COLOR_RGB(8, 8, 8)
#define FACE_TOP COLOR_RGB(248, 236, 192)
#define INK COLOR_RGB(40, 28, 16)
#define RED COLOR_RGB(160, 24, 16)
#define CREAM COLOR_RGB(240, 232, 216)
#define MUTED COLOR_RGB(168, 152, 128)
#define AMBER COLOR_RGB(248, 168, 48)
#define TRACK COLOR_RGB(72, 64, 56)
#define COVER_RED 0xF800

enum {
    LANDSCAPE,
    PORTRAIT,
    LARGE,
    LEFT,
    RIGHT,
    DIAL_PIVOT_X = 73,
    DIAL_PIVOT_Y = 112,
    PROBE_RADIUS = 78,
    DARK_GREEN = 24,
    INK_RED_MAX = 13,
    RED_RED_MIN = 17,
    BAR_LANDSCAPE_X = 84,
    BAR_LANDSCAPE_Y = 155,
    BAR_LANDSCAPE_WIDTH = 170,
    BAR_PORTRAIT_X = 10,
    BAR_PORTRAIT_Y = 298,
    BAR_PORTRAIT_WIDTH = 150,
    DURATION_MS = 245000,
    BAND_TICKS = 120,
    LANDSCAPE_BAND_ROWS = 40,
    PORTRAIT_BAND_ROWS = 47
};

typedef struct TextProbe {
    Rect label;
    Rect region;
    const char *text;
    Color color;
} TextProbe;

typedef struct Needles {
    uint32_t tick;
    int left;
    int right;
} Needles;
#define GOLDEN_LANDSCAPE_PAUSED 2026040173u
#define GOLDEN_PORTRAIT_PAUSED 1831269492u
#define GOLDEN_LARGE_PAUSED 1831269492u
#define GOLDEN_LANDSCAPE_PLAYING 3407851184u
#define GOLDEN_PORTRAIT_PLAYING 3471096605u

static Color pixels[NP_TEST_PIXELS];
static Color other[NP_TEST_PIXELS];
static Color expected[NP_TEST_PIXELS];

static const Needles GOLDEN[] = {
    {0, 46, 35},   {1, 48, 34},   {2, 40, 32},    {7, 38, 23},   {52, 17, 31},
    {53, 25, 33},  {33, 45, 24},  {54, 23, 35},   {106, 57, 35}, {107, 59, 36},
    {116, 38, 47}, {300, 29, 29}, {1396, 38, 47},
};

static NpTestScene scene_of(int layout, NpTestState state)
{
    return np_test_scene(NP_FACE_VU_METERS, NP_TEST_LAYOUTS[layout], state);
}

static Color at(const NpTestScene *scene, const Color *buffer, int x, int y)
{
    return np_test_pixel(buffer, scene->layout, x, y);
}

static int count_in(const NpTestScene *scene, const Color *buffer, Rect area, Color color)
{
    int count = 0;
    for (int y = area.y; y < area.y + area.height; y++) {
        for (int x = area.x; x < area.x + area.width; x++) {
            count += at(scene, buffer, x, y) == color ? 1 : 0;
        }
    }
    return count;
}

static Point dial_origin(const NpTestScene *scene, int side)
{
    if (scene->layout.width > scene->layout.height) {
        return (Point){side == LEFT ? 10 : 164, 28};
    }
    return (Point){12, scene->layout.area.y + 31};
}

static Point pivot_of(Point origin)
{
    return (Point){origin.x + DIAL_PIVOT_X, origin.y + DIAL_PIVOT_Y};
}

static bool has_near(const NpTestScene *scene, const Color *buffer, Point at_point, Color color)
{
    for (int y = at_point.y - 1; y <= at_point.y + 1; y++) {
        for (int x = at_point.x - 1; x <= at_point.x + 1; x++) {
            if (at(scene, buffer, x, y) == color) {
                return true;
            }
        }
    }
    return false;
}

static bool is_needle_pixel(Color pixel, Color kind)
{
    int red = pixel >> 11;
    int green = pixel >> 5 & 63;
    return green < DARK_GREEN && (kind == INK ? red <= INK_RED_MAX : red >= RED_RED_MIN);
}

static bool dark_near(const NpTestScene *scene, const Color *buffer, Point probe, Color kind)
{
    for (int y = probe.y - 1; y <= probe.y + 1; y++) {
        for (int x = probe.x - 1; x <= probe.x + 1; x++) {
            if (is_needle_pixel(at(scene, buffer, x, y), kind)) {
                return true;
            }
        }
    }
    return false;
}

static bool needle_points_at(const NpTestScene *scene, const Color *buffer, Point origin, int value,
                             Color kind)
{
    uint8_t angle = (uint8_t)(value * 7 / 8 - 28);
    return dark_near(scene, buffer, shape_polar(pivot_of(origin), angle, PROBE_RADIUS), kind);
}

static bool text_matches(const NpTestScene *scene, const TextProbe *probe)
{
    NpTestLayout layout = scene->layout;
    for (int index = 0; index < layout.width * layout.height; index++) {
        expected[index] = PLATE;
    }
    Canvas canvas = canvas_make(expected, (Rect){0, 0, layout.width, layout.height});
    label_draw(&canvas, (TextStyle){&TINY_FONT, probe->color}, (Label){probe->label, probe->text});
    for (int y = probe->region.y; y < probe->region.y + probe->region.height; y++) {
        for (int x = probe->region.x; x < probe->region.x + probe->region.width; x++) {
            if (at(scene, pixels, x, y) != np_test_pixel(expected, layout, x, y)) {
                return false;
            }
        }
    }
    return true;
}

static Rect centred(Rect row, const char *text)
{
    int width = text_width(&TINY_FONT, text);
    return (Rect){row.x + (row.width - width) / 2, row.y, width, row.height};
}

static Rect right_aligned(Rect row, const char *text)
{
    int width = text_width(&TINY_FONT, text);
    return (Rect){row.x + row.width - width, row.y, width, row.height};
}

static bool row_shows(const NpTestScene *scene, Rect row, const char *text, Color color)
{
    return text_matches(scene, &(TextProbe){centred(row, text), row, text, color});
}

static const char *volume_text(void)
{
    static char text[32];
    strcpy(text, i18n_text(STR_MENU_VOLUME));
    strcat(text, " 32");
    return text;
}

static void the_backdrop_colour_is_at_the_corner_of_the_area(void)
{
    for (int layout = LANDSCAPE; layout <= LARGE; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        np_test_draw(&scene, pixels);
        Rect area = scene.layout.area;
        CHECK_EQUAL(PLATE, at(&scene, pixels, area.x, area.y));
    }
}

static int needles_match(NpTestScene scene, const Needles *needles)
{
    scene.frame.tick = needles->tick;
    np_test_draw(&scene, pixels);
    Point left = dial_origin(&scene, LEFT);
    Point right = dial_origin(&scene, RIGHT);
    bool ok = needle_points_at(&scene, pixels, left, needles->left, INK) &&
              needle_points_at(&scene, pixels, right, needles->right, INK);
    return ok ? 1 : 0;
}

static void each_needle_follows_the_sway_formula_in_landscape(void)
{
    NpTestScene scene = scene_of(LANDSCAPE, NP_TEST_PLAYING);
    for (size_t index = 0; index < sizeof GOLDEN / sizeof GOLDEN[0]; index++) {
        CHECK_EQUAL(1, needles_match(scene, &GOLDEN[index]));
    }
}

static void a_needle_is_absent_from_the_angle_of_another_value(void)
{
    NpTestScene scene = scene_of(LANDSCAPE, NP_TEST_PLAYING);
    scene.frame.tick = 7;
    np_test_draw(&scene, pixels);
    Point origin = dial_origin(&scene, LEFT);
    CHECK(needle_points_at(&scene, pixels, origin, 38, INK));
    CHECK(!needle_points_at(&scene, pixels, origin, 38 + 24, INK));
    CHECK(!needle_points_at(&scene, pixels, origin, 38 - 24, INK));
}

static void the_stereo_dial_draws_an_ink_and_a_red_needle(void)
{
    for (int layout = PORTRAIT; layout <= LARGE; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        scene.frame.tick = 7;
        np_test_draw(&scene, pixels);
        Point origin = dial_origin(&scene, LEFT);
        CHECK(needle_points_at(&scene, pixels, origin, 38, INK));
        CHECK(needle_points_at(&scene, pixels, origin, 23, RED));
    }
}

static void a_paused_needle_rests_at_zero_whatever_the_tick(void)
{
    for (int layout = LANDSCAPE; layout <= LARGE; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PAUSED);
        scene.frame.tick = 107;
        np_test_draw(&scene, pixels);
        Color top = layout == LANDSCAPE ? INK : RED;
        CHECK(needle_points_at(&scene, pixels, dial_origin(&scene, LEFT), 0, top));
        CHECK(!needle_points_at(&scene, pixels, dial_origin(&scene, LEFT), 59, top));
    }
}

static void a_paused_frame_is_identical_at_any_tick(void)
{
    for (int layout = LANDSCAPE; layout <= LARGE; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PAUSED);
        np_test_draw(&scene, pixels);
        scene.frame.tick += 1;
        np_test_draw(&scene, other);
        CHECK(np_test_same(pixels, other, scene.layout));
    }
}

static void the_two_landscape_needles_move_independently(void)
{
    NpTestScene scene = scene_of(LANDSCAPE, NP_TEST_PLAYING);
    scene.frame.tick = 106;
    np_test_draw(&scene, pixels);
    CHECK(needle_points_at(&scene, pixels, dial_origin(&scene, LEFT), 57, INK));
    CHECK(needle_points_at(&scene, pixels, dial_origin(&scene, RIGHT), 35, INK));
    CHECK(!needle_points_at(&scene, pixels, dial_origin(&scene, RIGHT), 57, INK));
}

static bool lamp_is(const NpTestScene *scene, const Color *buffer, Point at_point, Color color)
{
    return at(scene, buffer, at_point.x, at_point.y) == color;
}

static void the_landscape_lamps_light_at_zero_vu_and_above(void)
{
    static const Needles CASES[] = {{106, 57, 35}, {116, 38, 47}, {0, 46, 35},
                                    {33, 45, 24},  {114, 43, 45}, {115, 40, 46}};
    static const bool LEFT_LIT[] = {true, false, true, false, false, false};
    static const bool RIGHT_LIT[] = {false, true, false, false, false, true};
    NpTestScene scene = scene_of(LANDSCAPE, NP_TEST_PLAYING);
    for (size_t index = 0; index < sizeof CASES / sizeof CASES[0]; index++) {
        scene.frame.tick = CASES[index].tick;
        np_test_draw(&scene, pixels);
        CHECK(lamp_is(&scene, pixels, (Point){10 + 131, 28 + 49}, LEFT_LIT[index] ? AMBER : TRACK));
        CHECK(
            lamp_is(&scene, pixels, (Point){164 + 131, 28 + 49}, RIGHT_LIT[index] ? AMBER : TRACK));
    }
}

static void the_landscape_lamps_are_dark_when_paused(void)
{
    NpTestScene scene = scene_of(LANDSCAPE, NP_TEST_PAUSED);
    scene.frame.tick = 106;
    np_test_draw(&scene, pixels);
    CHECK(lamp_is(&scene, pixels, (Point){141, 77}, TRACK));
    CHECK(lamp_is(&scene, pixels, (Point){295, 77}, TRACK));
}

static void the_stereo_lamp_lights_when_either_needle_reaches_zero_vu(void)
{
    NpTestScene scene = scene_of(PORTRAIT, NP_TEST_PLAYING);
    Point lamp = {dial_origin(&scene, LEFT).x + DIAL_PIVOT_X, dial_origin(&scene, LEFT).y + 63};
    static const Needles CASES[] = {{106, 57, 35}, {116, 38, 47}, {7, 38, 23}};
    static const Color EXPECTED[] = {AMBER, AMBER, TRACK};
    for (size_t index = 0; index < sizeof CASES / sizeof CASES[0]; index++) {
        scene.frame.tick = CASES[index].tick;
        np_test_draw(&scene, pixels);
        CHECK(lamp_is(&scene, pixels, lamp, EXPECTED[index]));
    }
}

static int changed_rows(const NpTestScene *scene, int *first, int *last)
{
    int rows = 0;
    *first = scene->layout.height;
    *last = -1;
    for (int y = 0; y < scene->layout.height; y++) {
        const Color *a = pixels + y * scene->layout.width;
        const Color *b = other + y * scene->layout.width;
        if (memcmp(a, b, (size_t)scene->layout.width * sizeof a[0]) != 0) {
            rows += 1;
            *first = y < *first ? y : *first;
            *last = y;
        }
    }
    return rows;
}

static void a_tick_changes_only_rows_inside_the_dials(void)
{
    for (int layout = LANDSCAPE; layout <= LARGE; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        int top = dial_origin(&scene, LEFT).y;
        int limit = layout == LANDSCAPE ? LANDSCAPE_BAND_ROWS : PORTRAIT_BAND_ROWS;
        int moved = 0;
        for (uint32_t tick = 0; tick < BAND_TICKS; tick++) {
            scene.frame.tick = tick;
            np_test_draw(&scene, pixels);
            scene.frame.tick = tick + 1;
            np_test_draw(&scene, other);
            int first;
            int last;
            int rows = changed_rows(&scene, &first, &last);
            CHECK(rows <= limit);
            CHECK(rows == 0 || (first >= top + 3 && last < top + 67));
            moved += rows > 0 ? 1 : 0;
        }
        CHECK(moved > BAND_TICKS / 2);
    }
}

static int fill_width(const NpTestScene *scene, const Color *buffer, int x, int y, int width,
                      Color color)
{
    return count_in(scene, buffer, (Rect){x, y, width, 1}, color);
}

static void the_progress_fill_grows_with_the_position(void)
{
    static const uint32_t SHOWN[] = {0, 20000, 60000, 139600, 200000, 245000};
    int previous = -1;
    for (size_t index = 0; index < sizeof SHOWN / sizeof SHOWN[0]; index++) {
        NpTestScene scene = scene_of(LANDSCAPE, NP_TEST_PLAYING);
        scene.frame.shown_ms = SHOWN[index];
        np_test_draw(&scene, pixels);
        int width = fill_width(&scene, pixels, BAR_LANDSCAPE_X, BAR_LANDSCAPE_Y,
                               BAR_LANDSCAPE_WIDTH, AMBER);
        CHECK(width > previous);
        CHECK_EQUAL((int)((uint64_t)SHOWN[index] * BAR_LANDSCAPE_WIDTH / DURATION_MS), width);
        previous = width;
    }
}

static void the_portrait_progress_fill_matches_the_position(void)
{
    NpTestScene scene = scene_of(PORTRAIT, NP_TEST_PLAYING);
    np_test_draw(&scene, pixels);
    int width =
        fill_width(&scene, pixels, BAR_PORTRAIT_X, BAR_PORTRAIT_Y, BAR_PORTRAIT_WIDTH, AMBER);
    CHECK_EQUAL(139600 * BAR_PORTRAIT_WIDTH / DURATION_MS, width);
    CHECK_EQUAL(BAR_PORTRAIT_WIDTH - width, fill_width(&scene, pixels, BAR_PORTRAIT_X,
                                                       BAR_PORTRAIT_Y, BAR_PORTRAIT_WIDTH, TRACK));
}

static void scrubbing_fills_the_bar_to_the_target_in_cream(void)
{
    NpTestScene scene = scene_of(LANDSCAPE, NP_TEST_SCRUBBING);
    np_test_draw(&scene, pixels);
    CHECK_EQUAL(
        201000 * BAR_LANDSCAPE_WIDTH / DURATION_MS,
        fill_width(&scene, pixels, BAR_LANDSCAPE_X, BAR_LANDSCAPE_Y, BAR_LANDSCAPE_WIDTH, CREAM));
    CHECK_EQUAL(0, fill_width(&scene, pixels, BAR_LANDSCAPE_X, BAR_LANDSCAPE_Y, BAR_LANDSCAPE_WIDTH,
                              AMBER));
}

static void the_volume_bar_is_cream_and_proportional_to_the_volume(void)
{
    for (int layout = LANDSCAPE; layout <= PORTRAIT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_VOLUME);
        bool landscape = layout == LANDSCAPE;
        int x = landscape ? BAR_LANDSCAPE_X : BAR_PORTRAIT_X;
        int y = landscape ? BAR_LANDSCAPE_Y : BAR_PORTRAIT_Y;
        int width = landscape ? BAR_LANDSCAPE_WIDTH : BAR_PORTRAIT_WIDTH;
        np_test_draw(&scene, pixels);
        CHECK_EQUAL(32 * width / 50, fill_width(&scene, pixels, x, y, width, CREAM));
    }
}

static void an_unknown_duration_draws_an_empty_bar_and_a_zero_remaining_time(void)
{
    NpTestScene scene = scene_of(LANDSCAPE, NP_TEST_PLAYING);
    scene.status.duration_ms = 0;
    np_test_draw(&scene, pixels);
    CHECK_EQUAL(0, fill_width(&scene, pixels, BAR_LANDSCAPE_X, BAR_LANDSCAPE_Y, BAR_LANDSCAPE_WIDTH,
                              AMBER));
    CHECK_EQUAL(BAR_LANDSCAPE_WIDTH, fill_width(&scene, pixels, BAR_LANDSCAPE_X, BAR_LANDSCAPE_Y,
                                                BAR_LANDSCAPE_WIDTH, TRACK));
    Rect remaining = {262, 147, 48, 16};
    CHECK(text_matches(&scene, &(TextProbe){right_aligned((Rect){10, 147, 300, 16}, "-0:00"),
                                            remaining, "-0:00", MUTED}));
}
static void the_time_texts_follow_the_shown_position(void)
{
    Rect row = {10, 147, 300, 16};
    NpTestScene scene = scene_of(LANDSCAPE, NP_TEST_PLAYING);
    np_test_draw(&scene, pixels);
    CHECK(text_matches(&scene, &(TextProbe){row, (Rect){10, 147, 52, 16}, "2:19", CREAM}));
    CHECK(text_matches(&scene, &(TextProbe){right_aligned(row, "-1:45"), (Rect){262, 147, 48, 16},
                                            "-1:45", MUTED}));
    scene.frame.shown_ms = 3700000;
    scene.status.duration_ms = 7300000;
    np_test_draw(&scene, pixels);
    CHECK(text_matches(&scene, &(TextProbe){row, (Rect){10, 147, 52, 16}, "1:01:40", CREAM}));
    CHECK(text_matches(&scene, &(TextProbe){right_aligned(row, "-1:00:00"),
                                            (Rect){262, 147, 48, 16}, "-1:00:00", MUTED}));
}
static void the_state_icon_is_an_amber_play_or_a_muted_pause(void)
{
    NpTestScene scene = scene_of(LANDSCAPE, NP_TEST_PLAYING);
    np_test_draw(&scene, pixels);
    CHECK_EQUAL(AMBER, at(&scene, pixels, 67, 154));
    scene = scene_of(LANDSCAPE, NP_TEST_PAUSED);
    np_test_draw(&scene, pixels);
    CHECK_EQUAL(MUTED, at(&scene, pixels, 66, 155));
    CHECK_EQUAL(MUTED, at(&scene, pixels, 75, 155));
    CHECK(at(&scene, pixels, 71, 155) != MUTED);
}

static void the_portrait_state_icon_sits_under_the_bar(void)
{
    NpTestScene scene = scene_of(PORTRAIT, NP_TEST_PLAYING);
    np_test_draw(&scene, pixels);
    CHECK_EQUAL(AMBER, at(&scene, pixels, 81, 309));
    scene = scene_of(PORTRAIT, NP_TEST_PAUSED);
    np_test_draw(&scene, pixels);
    CHECK_EQUAL(MUTED, at(&scene, pixels, 80, 310));
}

static void a_badge_draws_its_black_box_and_icon_over_the_dials(void)
{
    NpTestScene scene = scene_of(LANDSCAPE, NP_TEST_BADGE);
    np_test_draw(&scene, pixels);
    CHECK_EQUAL(TRACK, at(&scene, pixels, 140, 44));
    CHECK_EQUAL(BEZEL, at(&scene, pixels, 141, 45));
    CHECK_EQUAL(BEZEL, at(&scene, pixels, 178, 82));
    CHECK_EQUAL(CREAM, at(&scene, pixels, 167, 60));
    scene = scene_of(LANDSCAPE, NP_TEST_PLAYING);
    np_test_draw(&scene, pixels);
    CHECK(at(&scene, pixels, 141, 45) != BEZEL);
    CHECK(at(&scene, pixels, 140, 44) != TRACK);
}

static void each_badge_kind_draws_its_own_icon(void)
{
    static const NowPlayingFeedback KINDS[] = {
        NOW_PLAYING_FEEDBACK_NEXT, NOW_PLAYING_FEEDBACK_PREVIOUS, NOW_PLAYING_FEEDBACK_PAUSE,
        NOW_PLAYING_FEEDBACK_PLAY};
    static const Point LIT[] = {{167, 60}, {152, 60}, {152, 60}, {152, 63}};
    static const Point DARK[] = {{165, 60}, {168, 60}, {158, 60}, {168, 55}};
    for (size_t index = 0; index < sizeof KINDS / sizeof KINDS[0]; index++) {
        NpTestScene scene = scene_of(LANDSCAPE, NP_TEST_PLAYING);
        scene.frame.feedback = KINDS[index];
        np_test_draw(&scene, pixels);
        CHECK_EQUAL(CREAM, at(&scene, pixels, LIT[index].x, LIT[index].y));
        CHECK_EQUAL(BEZEL, at(&scene, pixels, DARK[index].x, DARK[index].y));
    }
}

static void the_portrait_badge_follows_the_dial_down_in_the_large_layout(void)
{
    NpTestScene standard = scene_of(PORTRAIT, NP_TEST_BADGE);
    NpTestScene large = scene_of(LARGE, NP_TEST_BADGE);
    np_test_draw(&standard, pixels);
    np_test_draw(&large, other);
    CHECK_EQUAL(TRACK, at(&standard, pixels, 65, 71 + 16));
    CHECK_EQUAL(TRACK, at(&large, other, 65, 77 + 16));
    CHECK(at(&standard, pixels, 65, 77 + 16 + 39) != TRACK);
}

static void the_dials_are_black_housings_with_cream_faces(void)
{
    for (int layout = LANDSCAPE; layout <= LARGE; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        Point origin = dial_origin(&scene, LEFT);
        np_test_draw(&scene, pixels);
        CHECK_EQUAL(BEZEL, at(&scene, pixels, origin.x + 70, origin.y + 68));
        CHECK_EQUAL(FACE_TOP, at(&scene, pixels, origin.x + 20, origin.y + 3));
        CHECK(at(&scene, pixels, origin.x + 20, origin.y + 1) == BEZEL);
        CHECK_EQUAL(PLATE, at(&scene, pixels, origin.x - 2, origin.y + 30));
    }
}

static void the_scale_is_ink_up_to_zero_and_red_beyond(void)
{
    for (int layout = LANDSCAPE; layout <= PORTRAIT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PAUSED);
        Point pivot = pivot_of(dial_origin(&scene, LEFT));
        np_test_draw(&scene, pixels);
        CHECK(has_near(&scene, pixels, shape_polar(pivot, (uint8_t)-10, 87), INK));
        CHECK(has_near(&scene, pixels, shape_polar(pivot, 18, 88), RED));
        CHECK(!has_near(&scene, pixels, shape_polar(pivot, 18, 87), INK));
        CHECK(dark_near(&scene, pixels, shape_polar(pivot, (uint8_t)-13, 92), INK));
        CHECK(!dark_near(&scene, pixels, shape_polar(pivot, (uint8_t)-13, 92), RED));
    }
}

static void the_landscape_has_two_dials_and_the_portrait_one(void)
{
    NpTestScene landscape = scene_of(LANDSCAPE, NP_TEST_PAUSED);
    NpTestScene portrait = scene_of(PORTRAIT, NP_TEST_PAUSED);
    np_test_draw(&landscape, pixels);
    CHECK_EQUAL(BEZEL, at(&landscape, pixels, 80, 96));
    CHECK_EQUAL(BEZEL, at(&landscape, pixels, 234, 96));
    CHECK_EQUAL(PLATE, at(&landscape, pixels, 160, 96));
    np_test_draw(&portrait, pixels);
    CHECK_EQUAL(BEZEL, at(&portrait, pixels, 12 + 70, 71 + 68));
    CHECK_EQUAL(PLATE, at(&portrait, pixels, 5, 71 + 68));
}

static void the_second_line_shows_the_feedback_before_the_lyric_before_the_artist(void)
{
    Rect row = {10, 121, 300, 20};
    NpTestScene scene = scene_of(LANDSCAPE, NP_TEST_PLAYING);
    np_test_draw(&scene, pixels);
    CHECK(row_shows(&scene, row, "The Long Way Home", MUTED));
    scene = scene_of(LANDSCAPE, NP_TEST_LYRIC);
    np_test_draw(&scene, pixels);
    CHECK(row_shows(&scene, row, "AB", CREAM));
    scene = scene_of(LANDSCAPE, NP_TEST_VOLUME);
    np_test_draw(&scene, pixels);
    CHECK(row_shows(&scene, row, volume_text(), CREAM));
    scene = scene_of(LANDSCAPE, NP_TEST_SCRUBBING);
    np_test_draw(&scene, pixels);
    CHECK(row_shows(&scene, row, i18n_text(STR_MENU_SEEKING_HINT), CREAM));
}
static void feedback_outranks_a_lyric_on_the_second_line(void)
{
    NpTestScene scene = scene_of(LANDSCAPE, NP_TEST_VOLUME);
    np_test_draw(&scene, pixels);
    NpTestScene lyric = scene;
    lyric.frame.lyric = "AB";
    np_test_draw(&lyric, other);
    CHECK(np_test_same(pixels, other, scene.layout));
}

static void the_portrait_rows_use_their_own_colours(void)
{
    NpTestScene scene = scene_of(PORTRAIT, NP_TEST_PAUSED);
    np_test_draw(&scene, pixels);
    CHECK(row_shows(&scene, (Rect){10, 174, 150, 22}, scene.frame.song, CREAM));
    CHECK(row_shows(&scene, (Rect){10, 196, 150, 22}, "The Long Way Home", MUTED));
    CHECK(row_shows(&scene, (Rect){10, 218, 150, 22}, "Analog Hours", MUTED));
    CHECK(row_shows(&scene, (Rect){10, 278, 150, 16}, "Shuffle songs", MUTED));
    CHECK(count_in(&scene, pixels, (Rect){10, 244, 150, 16}, MUTED) > 0);
    CHECK_EQUAL(0, count_in(&scene, pixels, (Rect){10, 244, 150, 16}, CREAM));
}
static void the_portrait_lyric_replaces_the_album_in_cream(void)
{
    NpTestScene scene = scene_of(PORTRAIT, NP_TEST_LYRIC);
    np_test_draw(&scene, pixels);
    CHECK(row_shows(&scene, (Rect){10, 218, 150, 22}, "AB", CREAM));
}
static void the_portrait_feedback_replaces_the_format_row_in_cream(void)
{
    Rect row = {10, 244, 150, 16};
    NpTestScene scene = scene_of(PORTRAIT, NP_TEST_SCRUBBING);
    np_test_draw(&scene, pixels);
    CHECK(row_shows(&scene, row, i18n_text(STR_MENU_SEEKING_HINT), CREAM));
    scene = scene_of(PORTRAIT, NP_TEST_VOLUME);
    np_test_draw(&scene, pixels);
    CHECK(row_shows(&scene, row, volume_text(), CREAM));
    CHECK_EQUAL(0, count_in(&scene, pixels, row, MUTED));
}
static void a_portrait_lyric_never_hides_the_feedback(void)
{
    NpTestScene scene = scene_of(PORTRAIT, NP_TEST_VOLUME);
    scene.frame.lyric = "AB";
    np_test_draw(&scene, pixels);
    CHECK(row_shows(&scene, (Rect){10, 244, 150, 16}, volume_text(), CREAM));
    CHECK(row_shows(&scene, (Rect){10, 218, 150, 22}, "AB", CREAM));
}
static void the_landscape_ignores_the_album_the_format_and_the_modes(void)
{
    NpTestScene scene = scene_of(LANDSCAPE, NP_TEST_PAUSED);
    np_test_draw(&scene, pixels);
    scene.frame.album = "BBBB";
    scene.frame.codec = "";
    scene.frame.stream = "";
    scene.frame.modes = "BBBBBBBB";
    np_test_draw(&scene, other);
    CHECK(np_test_same(pixels, other, scene.layout));
}

static void the_portrait_shows_the_modes_and_the_format(void)
{
    NpTestScene scene = scene_of(PORTRAIT, NP_TEST_PAUSED);
    np_test_draw(&scene, pixels);
    scene.frame.modes = "";
    scene.frame.codec = "";
    scene.frame.stream = "";
    np_test_draw(&scene, other);
    CHECK(!np_test_same(pixels, other, scene.layout));
    CHECK(count_in(&scene, other, (Rect){10, 278, 150, 16}, MUTED) == 0);
    CHECK(count_in(&scene, other, (Rect){10, 244, 150, 16}, MUTED) == 0);
}

static void the_cover_is_never_drawn(void)
{
    for (int layout = LANDSCAPE; layout <= LARGE; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        np_test_draw(&scene, pixels);
        CHECK_EQUAL(0, count_in(&scene, pixels, scene.layout.area, COVER_RED));
        NpTestScene bare = scene_of(layout, NP_TEST_NO_COVER);
        np_test_draw(&bare, other);
        CHECK(np_test_same(pixels, other, scene.layout));
    }
}

static bool differs_only_in(const NpTestScene *scene, Rect area)
{
    NpTestLayout layout = scene->layout;
    for (int y = 0; y < layout.height; y++) {
        for (int x = 0; x < layout.width; x++) {
            bool inside =
                x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height;
            if (!inside && at(scene, pixels, x, y) != at(scene, other, x, y)) {
                return false;
            }
        }
    }
    return true;
}

static void a_long_or_empty_title_changes_only_the_title_row(void)
{
    static const Rect TITLES[] = {{10, 102, 300, 20}, {10, 174, 150, 22}, {10, 174, 150, 22}};
    for (int layout = LANDSCAPE; layout <= LARGE; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PAUSED);
        np_test_draw(&scene, pixels);
        scene.frame.song = NP_TEST_LONG_TITLE;
        np_test_draw(&scene, other);
        CHECK(!np_test_same(pixels, other, scene.layout));
        CHECK(differs_only_in(&scene, TITLES[layout]));
        scene.frame.song = "";
        np_test_draw(&scene, other);
        CHECK(differs_only_in(&scene, TITLES[layout]));
        CHECK_EQUAL(0, count_in(&scene, other, TITLES[layout], CREAM));
    }
}

static void a_long_title_scrolls_while_playing_and_stands_still_when_paused(void)
{
    static const char LONG_TITLE[] =
        "ABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABAB"
        "ABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABAB";
    NpTestScene scene = scene_of(LANDSCAPE, NP_TEST_PLAYING);
    scene.frame.song = LONG_TITLE;
    scene.frame.now_ms = scene.frame.text_ms + 3000;
    np_test_draw(&scene, pixels);
    scene.frame.now_ms += 100;
    np_test_draw(&scene, other);
    CHECK(!np_test_same(pixels, other, scene.layout));
    CHECK(differs_only_in(&scene, (Rect){10, 102, 300, 20}));
    scene.frame.playing = false;
    np_test_draw(&scene, pixels);
    scene.frame.now_ms += 100;
    np_test_draw(&scene, other);
    CHECK(np_test_same(pixels, other, scene.layout));
}
static uint32_t region_hash(const NpTestScene *scene, Rect area)
{
    uint32_t hash = 2166136261u;
    for (int y = area.y; y < area.y + area.height; y++) {
        for (int x = area.x; x < area.x + area.width; x++) {
            hash = (hash ^ at(scene, pixels, x, y)) * 16777619u;
        }
    }
    return hash;
}

static uint32_t dial_hash(int layout, NpTestState state, uint32_t tick)
{
    NpTestScene scene = scene_of(layout, state);
    scene.frame.tick = tick;
    np_test_draw(&scene, pixels);
    Point origin = dial_origin(&scene, LEFT);
    int width = layout == LANDSCAPE ? 308 : 146;
    return region_hash(&scene, (Rect){origin.x, origin.y, width, 72});
}

static void the_dial_geometry_and_scale_match_the_approved_rendering(void)
{
    CHECK_EQUAL(GOLDEN_LANDSCAPE_PAUSED, dial_hash(LANDSCAPE, NP_TEST_PAUSED, 0));
    CHECK_EQUAL(GOLDEN_PORTRAIT_PAUSED, dial_hash(PORTRAIT, NP_TEST_PAUSED, 0));
    CHECK_EQUAL(GOLDEN_LARGE_PAUSED, dial_hash(LARGE, NP_TEST_PAUSED, 0));
    CHECK_EQUAL(GOLDEN_LANDSCAPE_PLAYING, dial_hash(LANDSCAPE, NP_TEST_PLAYING, 106));
    CHECK_EQUAL(GOLDEN_PORTRAIT_PLAYING, dial_hash(PORTRAIT, NP_TEST_PLAYING, 116));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_backdrop_colour_is_at_the_corner_of_the_area),
        TEST_CASE(each_needle_follows_the_sway_formula_in_landscape),
        TEST_CASE(a_needle_is_absent_from_the_angle_of_another_value),
        TEST_CASE(the_stereo_dial_draws_an_ink_and_a_red_needle),
        TEST_CASE(a_paused_needle_rests_at_zero_whatever_the_tick),
        TEST_CASE(a_paused_frame_is_identical_at_any_tick),
        TEST_CASE(the_two_landscape_needles_move_independently),
        TEST_CASE(the_landscape_lamps_light_at_zero_vu_and_above),
        TEST_CASE(the_landscape_lamps_are_dark_when_paused),
        TEST_CASE(the_stereo_lamp_lights_when_either_needle_reaches_zero_vu),
        TEST_CASE(a_tick_changes_only_rows_inside_the_dials),
        TEST_CASE(the_progress_fill_grows_with_the_position),
        TEST_CASE(the_portrait_progress_fill_matches_the_position),
        TEST_CASE(scrubbing_fills_the_bar_to_the_target_in_cream),
        TEST_CASE(the_volume_bar_is_cream_and_proportional_to_the_volume),
        TEST_CASE(an_unknown_duration_draws_an_empty_bar_and_a_zero_remaining_time),
        TEST_CASE(the_time_texts_follow_the_shown_position),
        TEST_CASE(the_state_icon_is_an_amber_play_or_a_muted_pause),
        TEST_CASE(the_portrait_state_icon_sits_under_the_bar),
        TEST_CASE(a_badge_draws_its_black_box_and_icon_over_the_dials),
        TEST_CASE(each_badge_kind_draws_its_own_icon),
        TEST_CASE(the_portrait_badge_follows_the_dial_down_in_the_large_layout),
        TEST_CASE(the_dials_are_black_housings_with_cream_faces),
        TEST_CASE(the_scale_is_ink_up_to_zero_and_red_beyond),
        TEST_CASE(the_landscape_has_two_dials_and_the_portrait_one),
        TEST_CASE(the_second_line_shows_the_feedback_before_the_lyric_before_the_artist),
        TEST_CASE(feedback_outranks_a_lyric_on_the_second_line),
        TEST_CASE(the_portrait_rows_use_their_own_colours),
        TEST_CASE(the_portrait_lyric_replaces_the_album_in_cream),
        TEST_CASE(the_portrait_feedback_replaces_the_format_row_in_cream),
        TEST_CASE(a_portrait_lyric_never_hides_the_feedback),
        TEST_CASE(the_landscape_ignores_the_album_the_format_and_the_modes),
        TEST_CASE(the_portrait_shows_the_modes_and_the_format),
        TEST_CASE(the_cover_is_never_drawn),
        TEST_CASE(a_long_or_empty_title_changes_only_the_title_row),
        TEST_CASE(a_long_title_scrolls_while_playing_and_stands_still_when_paused),
        TEST_CASE(the_dial_geometry_and_scale_match_the_approved_rendering),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

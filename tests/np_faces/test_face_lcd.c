#include "check.h"
#include "gfx/text.h"
#include "np_faces_test.h"
#include "tiny_font.h"

enum {
    INK = COLOR_RGB(24, 36, 16),
    GLASS = COLOR_RGB(176, 196, 104),
    GHOST = COLOR_RGB(144, 168, 80),
    SHADOW = COLOR_RGB(136, 156, 72),
    BEZEL = COLOR_RGB(32, 32, 40),
    COVER_RED = 0xF800,
    GHOST_GREEN = 40,
    TOP_INSET = 3,
    LEGEND_COUNT = 5,
    LEGEND_WIDTH = 12,
    LEGEND_HEIGHT = 15,
    BADGE_SIDE = 18,
    BAND_CELLS = 8,
    BAND_PITCH = 9,
    TICK_PAIRS = 60,
    MAX_BAND_ROWS = 24,
    DIGIT_SLOTS = 5,
    COLON_WIDTH = 4,
    COLON_HEIGHT = 40,
    SHOWN_1_23_45 = 5025000,
    SHOWN_0_05 = 5000,
    SHOWN_0_83 = 83000,
    SCRUB_MS = 201000,
    SHOWN_OVER = 40000000,
    SHOWN_MAX = 35999000,
    VOLUME_SEVEN = 7,
    VOLUME_88 = 88,
    VOLUME_OVER = 200,
    POSITION_STEP_MS = 20000,
    BAND_ROW_FIRST = 35,
    LYRIC_LAND_Y = 90,
    LYRIC_PORT_Y = 214,
    TEXT_HEIGHT = 20,
    ARTIST_PORT_Y = 194,
    COLON_DOT_WIDTH = 4,
    COLON_LEFT_UPPER = 27,
    COLON_LEFT_LOWER = 25,
    COLON_RIGHT_UPPER = 83,
    COLON_RIGHT_LOWER = 81,
    COLON_UPPER_ROW = 9,
    COLON_LOWER_ROW = 27,
    SHOWN_8_08_08 = 29288000,
    BIG_BOX_WIDTH = 150,
    BIG_BOX_HEIGHT = 46,
    BIG_LAST_COLUMN = 137,
    BIG_LAST_ROW = 39,
    LEGEND_SCAN = 40,
    INFO_GAP = 8
};

typedef struct Spot {
    int glass_x;
    int bands;
    int spectrum_x;
    int spectrum_base;
    int cells;
    Point title;
    Point progress;
    Point big;
    Point track;
    Point badge;
    Point codec;
    Point info;
    int info_width;
    int play_x;
    int pause_x;
    int lyric_y;
    Point blank;
    Point legends[LEGEND_COUNT];
} Spot;

typedef struct ModeRow {
    ShuffleMode shuffle;
    RepeatMode repeat;
    bool lit[LEGEND_COUNT];
} ModeRow;

typedef struct Segments {
    bool top;
    bool middle;
    bool bottom;
} Segments;

static const Spot SPOTS[2] = {
    {5,
     10,
     220,
     60,
     74,
     {12, 68},
     {12, 115},
     {68, 23},
     {12, 39},
     {255, 35},
     {308, 4},
     {12, 122},
     296,
     12,
     25,
     LYRIC_LAND_Y,
     {200, 65},
     {{41, 4}, {101, 4}, {156, 4}, {208, 4}, {219, 4}}},
    {4,
     16,
     14,
     160,
     37,
     {10, 171},
     {11, 241},
     {16, 76},
     {85, 44},
     {76, 134},
     {160, 21},
     {10, 248},
     150,
     10,
     23,
     LYRIC_PORT_Y,
     {80, 166},
     {{40, 4}, {100, 4}, {10, 21}, {62, 21}, {73, 21}}},
};

static const char GOLD_LANDSCAPE[4][11] = {"8877744452", "8887636232", "7564644543", "6784626544"};
static const char GOLD_PORTRAIT[4][17] = {"8878788863433253", "8888668643244432",
                                          "7565655654644345", "6785637655434553"};
static const uint32_t GOLD_TICKS[4] = {0, 1, 1396, 1397};

static const ModeRow MODES[] = {
    {SHUFFLE_OFF, REPEAT_OFF, {false, false, false, false, false}},
    {SHUFFLE_SONGS, REPEAT_OFF, {true, false, false, false, false}},
    {SHUFFLE_ALBUMS, REPEAT_OFF, {true, true, false, false, false}},
    {SHUFFLE_OFF, REPEAT_ONE, {false, false, true, true, false}},
    {SHUFFLE_OFF, REPEAT_ALL, {false, false, true, false, true}},
};

static const Segments DIGIT_SEGMENTS[10] = {
    [0] = {true, false, true}, [1] = {false, false, false}, [2] = {true, true, true},
    [3] = {true, true, true},  [4] = {false, true, false},  [5] = {true, true, true},
    [6] = {true, true, true},  [7] = {true, false, false},  [8] = {true, true, true},
    [9] = {true, true, true},
};

static const uint8_t BIG_PITCH[DIGIT_SLOTS] = {0, 30, 56, 86, 112};

static Color pixels[NP_TEST_PIXELS];
static Color other[NP_TEST_PIXELS];

static const Spot *spot_of(int layout)
{
    return &SPOTS[layout > 0 ? 1 : 0];
}

static int top_of(int layout)
{
    return NP_TEST_LAYOUTS[layout].area.y + TOP_INSET;
}

static Color at(const Color *grid, int layout, Point point)
{
    return np_test_pixel(grid, NP_TEST_LAYOUTS[layout], point.x, top_of(layout) + point.y);
}

static NpTestScene scene_of(int layout, NpTestState state)
{
    return np_test_scene(NP_FACE_LCD, NP_TEST_LAYOUTS[layout], state);
}

static int count_in(const Color *grid, int layout, Rect area, Color colour)
{
    int count = 0;
    for (int y = 0; y < area.height; y++) {
        for (int x = 0; x < area.width; x++) {
            Point point = {area.x + x, area.y + y};
            count += at(grid, layout, point) == colour ? 1 : 0;
        }
    }
    return count;
}

static int count_dark(const Color *grid, int layout, Rect area)
{
    int count = 0;
    for (int y = 0; y < area.height; y++) {
        for (int x = 0; x < area.width; x++) {
            Color colour = at(grid, layout, (Point){area.x + x, area.y + y});
            count += ((colour >> 5) & 0x3F) < GHOST_GREEN ? 1 : 0;
        }
    }
    return count;
}

static void draw_into(const NpTestScene *scene, Color *grid)
{
    np_test_draw(scene, grid);
}

static bool row_differs(int layout, int row)
{
    NpTestLayout shape = NP_TEST_LAYOUTS[layout];
    for (int x = 0; x < shape.width; x++) {
        if (np_test_pixel(pixels, shape, x, row) != np_test_pixel(other, shape, x, row)) {
            return true;
        }
    }
    return false;
}

static bool rows_within(int layout, int first, int last)
{
    NpTestLayout shape = NP_TEST_LAYOUTS[layout];
    for (int row = 0; row < shape.height; row++) {
        if (row_differs(layout, row) &&
            (row < top_of(layout) + first || row > top_of(layout) + last)) {
            return false;
        }
    }
    return true;
}

static int differing_rows(int layout)
{
    int count = 0;
    for (int row = 0; row < NP_TEST_LAYOUTS[layout].height; row++) {
        count += row_differs(layout, row) ? 1 : 0;
    }
    return count;
}

static void the_glass_has_a_lip_and_cut_corners_in_a_bezel(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        const Spot *spot = spot_of(layout);
        int bottom = NP_TEST_LAYOUTS[layout].area.height - TOP_INSET * 2 - 1;
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        draw_into(&scene, pixels);
        CHECK_EQUAL(BEZEL, at(pixels, layout, (Point){spot->glass_x, 0}));
        CHECK_EQUAL(BEZEL, at(pixels, layout, (Point){spot->glass_x, bottom}));
        CHECK_EQUAL(BEZEL, at(pixels, layout, (Point){spot->glass_x - 1, 40}));
        CHECK_EQUAL(SHADOW, at(pixels, layout, (Point){spot->glass_x + 1, 1}));
        CHECK_EQUAL(GLASS, at(pixels, layout, (Point){spot->glass_x + 1, 2}));
        CHECK_EQUAL(GLASS, at(pixels, layout, spot->blank));
    }
}

static void the_play_and_pause_legends_follow_the_state(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        const Spot *spot = spot_of(layout);
        Point play = {spot->play_x, 11};
        Point pause = {spot->pause_x, 7};
        NpTestScene playing = scene_of(layout, NP_TEST_PLAYING);
        NpTestScene paused = scene_of(layout, NP_TEST_PAUSED);
        draw_into(&playing, pixels);
        draw_into(&paused, other);
        CHECK_EQUAL(INK, at(pixels, layout, play));
        CHECK_EQUAL(GHOST, at(pixels, layout, pause));
        CHECK_EQUAL(GHOST, at(other, layout, play));
        CHECK_EQUAL(INK, at(other, layout, pause));
    }
}

static void the_mode_legends_light_only_for_their_modes(void)
{
    for (int layout = 0; layout < 2; layout++) {
        for (size_t row = 0; row < sizeof MODES / sizeof MODES[0]; row++) {
            NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
            scene.status.shuffle = MODES[row].shuffle;
            scene.status.repeat = MODES[row].repeat;
            draw_into(&scene, pixels);
            for (int legend = 0; legend < LEGEND_COUNT; legend++) {
                Point corner = spot_of(layout)->legends[legend];
                Rect box = {corner.x, corner.y, LEGEND_WIDTH, LEGEND_HEIGHT};
                CHECK_EQUAL(MODES[row].lit[legend], count_dark(pixels, layout, box) > 0);
            }
        }
    }
}

static Segments segments_at(const Color *grid, int layout, Point origin, bool big)
{
    Point top = big ? (Point){origin.x + 15, origin.y + 2} : (Point){origin.x + 8, origin.y + 1};
    Point middle =
        big ? (Point){origin.x + 13, origin.y + 20} : (Point){origin.x + 7, origin.y + 12};
    Point bottom =
        big ? (Point){origin.x + 11, origin.y + 37} : (Point){origin.x + 6, origin.y + 22};
    return (Segments){at(grid, layout, top) == INK, at(grid, layout, middle) == INK,
                      at(grid, layout, bottom) == INK};
}

static bool shows_digit(const Color *grid, int layout, int slot, int digit)
{
    Point big = spot_of(layout)->big;
    Segments seen = segments_at(grid, layout, (Point){big.x + BIG_PITCH[slot], big.y}, true);
    Segments want = DIGIT_SEGMENTS[digit];
    return seen.top == want.top && seen.middle == want.middle && seen.bottom == want.bottom;
}

static bool shows_nothing(const Color *grid, int layout, int slot)
{
    Point big = spot_of(layout)->big;
    Segments seen = segments_at(grid, layout, (Point){big.x + BIG_PITCH[slot], big.y}, true);
    return !seen.top && !seen.middle && !seen.bottom;
}

static void the_big_digits_show_hours_minutes_and_seconds(void)
{
    static const int DIGITS[DIGIT_SLOTS] = {1, 2, 3, 4, 5};
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        scene.frame.shown_ms = SHOWN_1_23_45;
        draw_into(&scene, pixels);
        CHECK(shows_nothing(pixels, layout, 0));
        for (int slot = 1; slot < DIGIT_SLOTS; slot++) {
            CHECK(shows_digit(pixels, layout, slot, DIGITS[slot]));
        }
    }
}

static void leading_zeros_stay_ghosted_and_the_seconds_always_show(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        scene.frame.shown_ms = SHOWN_0_05;
        draw_into(&scene, pixels);
        CHECK(shows_nothing(pixels, layout, 0));
        CHECK(shows_nothing(pixels, layout, 1));
        CHECK(shows_digit(pixels, layout, 2, 0));
        CHECK(shows_digit(pixels, layout, 3, 0));
        CHECK(shows_digit(pixels, layout, 4, 5));
    }
}

static void the_digits_follow_the_scrub_target_not_the_position(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_SCRUBBING);
        scene.status.position_ms = 0;
        draw_into(&scene, pixels);
        CHECK(shows_digit(pixels, layout, 2, 3));
        CHECK(shows_digit(pixels, layout, 3, 2));
        CHECK(shows_digit(pixels, layout, 4, 1));
    }
}

static void times_beyond_nine_hours_clamp_to_the_last_displayable_second(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        scene.frame.shown_ms = SHOWN_OVER;
        draw_into(&scene, pixels);
        scene.frame.shown_ms = SHOWN_MAX;
        draw_into(&scene, other);
        CHECK(np_test_same(pixels, other, NP_TEST_LAYOUTS[layout]));
        CHECK(shows_digit(pixels, layout, 0, 9));
    }
}

static Rect colon_box(const Spot *spot, int dx)
{
    return (Rect){spot->big.x + dx, 0, COLON_WIDTH, COLON_HEIGHT};
}

static int colon_ink(const Color *grid, int layout, int dx)
{
    Rect box = colon_box(spot_of(layout), dx);
    box.y = spot_of(layout)->big.y;
    return count_in(grid, layout, box, INK);
}

static void the_hours_colon_is_lit_only_with_hours(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        scene.frame.shown_ms = SHOWN_1_23_45;
        draw_into(&scene, pixels);
        scene.frame.shown_ms = SHOWN_0_83;
        draw_into(&scene, other);
        CHECK(colon_ink(pixels, layout, 22) > 0);
        CHECK_EQUAL(0, colon_ink(other, layout, 22));
        CHECK(colon_ink(other, layout, 78) > 0);
    }
}

static void volume_shows_its_level_in_the_last_two_digits_with_both_colons_off(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_VOLUME);
        scene.status.volume = VOLUME_SEVEN;
        draw_into(&scene, pixels);
        scene.status.volume = VOLUME_88;
        draw_into(&scene, other);
        CHECK(shows_nothing(pixels, layout, 0) && shows_nothing(pixels, layout, 3));
        CHECK(shows_digit(pixels, layout, 4, 7));
        CHECK(shows_digit(other, layout, 3, 8) && shows_digit(other, layout, 4, 8));
        CHECK_EQUAL(0, colon_ink(pixels, layout, 22) + colon_ink(pixels, layout, 78));
    }
}

static void a_volume_beyond_ninety_nine_shows_ninety_nine(void)
{
    NpTestScene scene = scene_of(0, NP_TEST_VOLUME);
    scene.status.volume = VOLUME_OVER;
    draw_into(&scene, pixels);
    CHECK(shows_digit(pixels, 0, 3, 9) && shows_digit(pixels, 0, 4, 9));
}

static void the_track_number_fills_the_medium_digits_without_leading_zeros(void)
{
    static const unsigned NUMBERS[3] = {3, 0, 1000};
    static const int LAST[3] = {3, 0, 9};
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        for (int index = 0; index < 3; index++) {
            NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
            scene.status.place.number = (uint16_t)NUMBERS[index];
            draw_into(&scene, pixels);
            Point track = spot_of(layout)->track;
            Segments last = segments_at(pixels, layout, (Point){track.x + 32, track.y}, false);
            Segments first = segments_at(pixels, layout, track, false);
            CHECK(last.top == DIGIT_SEGMENTS[LAST[index]].top);
            CHECK(last.middle == DIGIT_SEGMENTS[LAST[index]].middle);
            CHECK_EQUAL(NUMBERS[index] >= 100, first.top);
        }
    }
}

static int level_at(const Color *grid, int layout, int band)
{
    const Spot *spot = spot_of(layout);
    int level = 0;
    for (int cell = 0; cell < BAND_CELLS; cell++) {
        Point point = {spot->spectrum_x + BAND_PITCH * band + 3,
                       spot->spectrum_base - 5 * cell + 1};
        level += at(grid, layout, point) == INK ? 1 : 0;
    }
    return level;
}

static bool cells_rise_from_the_bottom(const Color *grid, int layout, int band)
{
    const Spot *spot = spot_of(layout);
    bool gap = false;
    for (int cell = 0; cell < BAND_CELLS; cell++) {
        Point point = {spot->spectrum_x + BAND_PITCH * band + 3,
                       spot->spectrum_base - 5 * cell + 1};
        Color colour = at(grid, layout, point);
        gap = gap || colour == GHOST;
        if (colour != INK && colour != GHOST) {
            return false;
        }
        if (gap && colour == INK) {
            return false;
        }
    }
    return true;
}

static void the_spectrum_levels_match_the_reference_hash(void)
{
    for (int layout = 0; layout < 2; layout++) {
        for (int tick = 0; tick < 4; tick++) {
            NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
            scene.frame.tick = GOLD_TICKS[tick];
            draw_into(&scene, pixels);
            const char *gold = layout == 0 ? GOLD_LANDSCAPE[tick] : GOLD_PORTRAIT[tick];
            for (int band = 0; band < spot_of(layout)->bands; band++) {
                CHECK_EQUAL(gold[band] - '0', level_at(pixels, layout, band));
                CHECK(cells_rise_from_the_bottom(pixels, layout, band));
            }
        }
    }
}

static void a_paused_spectrum_has_every_cell_off(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PAUSED);
        draw_into(&scene, pixels);
        for (int band = 0; band < spot_of(layout)->bands; band++) {
            CHECK_EQUAL(0, level_at(pixels, layout, band));
            CHECK(cells_rise_from_the_bottom(pixels, layout, band));
        }
    }
}

static void a_paused_face_ignores_the_tick(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PAUSED);
        draw_into(&scene, pixels);
        scene.frame.tick += 7;
        draw_into(&scene, other);
        CHECK(np_test_same(pixels, other, NP_TEST_LAYOUTS[layout]));
    }
}

static void the_next_tick_changes_only_spectrum_rows(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        int changed = 0;
        for (int tick = 0; tick < TICK_PAIRS; tick++) {
            NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
            scene.frame.tick = (uint32_t)tick;
            draw_into(&scene, pixels);
            scene.frame.tick = (uint32_t)tick + 1;
            draw_into(&scene, other);
            CHECK(differing_rows(layout) <= MAX_BAND_ROWS);
            CHECK(rows_within(layout, spot_of(layout)->spectrum_base - BAND_ROW_FIRST,
                              spot_of(layout)->spectrum_base + 2));
            changed += differing_rows(layout);
        }
        CHECK(changed > 0);
    }
}

static void a_badge_replaces_the_spectrum_with_an_icon(void)
{
    static const NowPlayingFeedback KINDS[] = {
        NOW_PLAYING_FEEDBACK_NEXT, NOW_PLAYING_FEEDBACK_PREVIOUS, NOW_PLAYING_FEEDBACK_PAUSE,
        NOW_PLAYING_FEEDBACK_PLAY};
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        const Spot *spot = spot_of(layout);
        Rect box = {spot->badge.x, spot->badge.y, BADGE_SIDE, BADGE_SIDE};
        Point cell = {spot->spectrum_x + 3, spot->spectrum_base + 1};
        NpTestScene plain = scene_of(layout, NP_TEST_PLAYING);
        draw_into(&plain, other);
        CHECK(at(other, layout, cell) != GLASS);
        for (size_t kind = 0; kind < sizeof KINDS / sizeof KINDS[0]; kind++) {
            NpTestScene scene = plain;
            scene.frame.feedback = KINDS[kind];
            draw_into(&scene, pixels);
            CHECK(count_in(pixels, layout, box, INK) > 0);
            CHECK_EQUAL(GLASS, at(pixels, layout, cell));
        }
    }
}

static void the_title_sits_in_a_dotted_field(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        Point title = spot_of(layout)->title;
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        scene.frame.song = "";
        draw_into(&scene, pixels);
        CHECK_EQUAL(GHOST, at(pixels, layout, (Point){title.x, title.y}));
        CHECK_EQUAL(GHOST, at(pixels, layout, (Point){title.x + 3, title.y + 3}));
        CHECK_EQUAL(GLASS, at(pixels, layout, (Point){title.x + 2, title.y + 3}));
        CHECK_EQUAL(GLASS, at(pixels, layout, (Point){title.x + 3, title.y + 2}));
        CHECK_EQUAL(GLASS, at(pixels, layout, (Point){title.x + 5, title.y + 3}));
        CHECK_EQUAL(GHOST, at(pixels, layout, (Point){title.x + 4, title.y + 4}));
        CHECK_EQUAL(GLASS, at(pixels, layout, (Point){title.x + 3, title.y + 5}));
        CHECK_EQUAL(GLASS, at(pixels, layout, (Point){title.x, title.y - 1}));
        CHECK_EQUAL(GLASS, at(pixels, layout, (Point){title.x, title.y + TEXT_HEIGHT}));
        CHECK_EQUAL(0, count_dark(pixels, layout, (Rect){title.x, title.y, 100, TEXT_HEIGHT}));
    }
}

static void the_title_text_is_drawn_in_ink_over_the_field(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        Point title = spot_of(layout)->title;
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        draw_into(&scene, pixels);
        CHECK(count_dark(pixels, layout, (Rect){title.x, title.y, 150, TEXT_HEIGHT}) > 0);
    }
}

static int lit_cells(const Color *grid, int layout)
{
    const Spot *spot = spot_of(layout);
    int lit = 0;
    for (int cell = 0; cell < spot->cells; cell++) {
        Point point = {spot->progress.x + 4 * cell + 1, spot->progress.y + 1};
        lit += at(grid, layout, point) == INK ? 1 : 0;
    }
    return lit;
}

static int expected_cells(uint32_t position, uint32_t duration, int cells)
{
    if (duration == 0) {
        return 0;
    }
    return position >= duration ? cells : (int)((uint64_t)position * (uint64_t)cells / duration);
}

static void the_progress_cells_follow_the_shown_position(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        int cells = spot_of(layout)->cells;
        int previous = 0;
        for (uint32_t shown = 0; shown <= scene.status.duration_ms + 1; shown += POSITION_STEP_MS) {
            scene.frame.shown_ms = shown;
            draw_into(&scene, pixels);
            CHECK_EQUAL(expected_cells(shown, scene.status.duration_ms, cells),
                        lit_cells(pixels, layout));
            CHECK(lit_cells(pixels, layout) >= previous);
            previous = lit_cells(pixels, layout);
        }
        scene.frame.shown_ms = scene.status.duration_ms;
        draw_into(&scene, pixels);
        CHECK_EQUAL(cells, lit_cells(pixels, layout));
    }
}

static void the_progress_cells_are_three_wide_with_a_gap(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        Point cell = spot_of(layout)->progress;
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        scene.frame.shown_ms = scene.status.duration_ms;
        draw_into(&scene, pixels);
        CHECK_EQUAL(INK, at(pixels, layout, (Point){cell.x + 2, cell.y + 3}));
        CHECK_EQUAL(GLASS, at(pixels, layout, (Point){cell.x + 3, cell.y + 1}));
        CHECK_EQUAL(GLASS, at(pixels, layout, (Point){cell.x + 1, cell.y + 4}));
    }
}

static void an_unknown_duration_lights_no_cell(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        scene.status.duration_ms = 0;
        draw_into(&scene, pixels);
        CHECK_EQUAL(0, lit_cells(pixels, layout));
    }
}

static void volume_and_scrubbing_drive_the_cells(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        int cells = spot_of(layout)->cells;
        NpTestScene volume = scene_of(layout, NP_TEST_VOLUME);
        NpTestScene scrub = scene_of(layout, NP_TEST_SCRUBBING);
        draw_into(&volume, pixels);
        draw_into(&scrub, other);
        CHECK_EQUAL(volume.status.volume * cells / 50, lit_cells(pixels, layout));
        CHECK_EQUAL(expected_cells(SCRUB_MS, scrub.status.duration_ms, cells),
                    lit_cells(other, layout));
    }
}

static Rect info_box(int layout, int left, int width)
{
    const Spot *spot = spot_of(layout);
    return (Rect){spot->info.x + left, spot->info.y, width, LEGEND_HEIGHT};
}

static void the_info_row_has_the_stream_left_and_the_remaining_time_right(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        int width = spot_of(layout)->info_width;
        draw_into(&scene, pixels);
        scene.frame.stream = "";
        draw_into(&scene, other);
        CHECK(count_dark(pixels, layout, info_box(layout, 0, 40)) > 0);
        CHECK_EQUAL(0, count_dark(other, layout, info_box(layout, 0, 40)));
        CHECK(count_dark(other, layout, info_box(layout, width - 12, 12)) > 0);
    }
}

static void volume_and_scrubbing_replace_the_whole_info_row(void)
{
    static const NpTestState STATES[2] = {NP_TEST_VOLUME, NP_TEST_SCRUBBING};
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        for (int state = 0; state < 2; state++) {
            NpTestScene scene = scene_of(layout, STATES[state]);
            scene.frame.stream = "BBBBBBBBBBBB";
            draw_into(&scene, pixels);
            scene.frame.stream = "A";
            scene.status.duration_ms = scene.status.duration_ms == 0 ? 1 : scene.status.duration_ms;
            draw_into(&scene, other);
            CHECK(np_test_same(pixels, other, NP_TEST_LAYOUTS[layout]));
        }
    }
}

static void the_codec_box_hugs_its_text_and_hides_when_unknown(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        Point corner = spot_of(layout)->codec;
        int width = text_width(&TINY_FONT, "FLAC") + 8;
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        draw_into(&scene, pixels);
        CHECK_EQUAL(INK, at(pixels, layout, (Point){corner.x - 1, corner.y + 7}));
        CHECK_EQUAL(INK, at(pixels, layout, (Point){corner.x - width, corner.y + 7}));
        CHECK_EQUAL(GLASS, at(pixels, layout, (Point){corner.x - width - 1, corner.y + 7}));
        scene.frame.codec = "";
        draw_into(&scene, pixels);
        CHECK_EQUAL(GLASS, at(pixels, layout, (Point){corner.x - 1, corner.y + 7}));
    }
}

static void a_lyric_replaces_only_the_artist_or_album_row(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        draw_into(&scene, pixels);
        scene.frame.lyric = "AB";
        draw_into(&scene, other);
        int row = spot_of(layout)->lyric_y;
        CHECK(differing_rows(layout) > 0);
        CHECK(rows_within(layout, row, row + TEXT_HEIGHT - 1));
    }
}

typedef struct Span {
    int first;
    int last;
} Span;

static Span dark_span(const Color *grid, int layout, Rect area)
{
    Span span = {area.width, -1};
    for (int y = 0; y < area.height; y++) {
        for (int x = 0; x < area.width; x++) {
            Color colour = at(grid, layout, (Point){area.x + x, area.y + y});
            bool hit = ((colour >> 5) & 0x3F) < GHOST_GREEN;
            span.first = hit && x < span.first ? x : span.first;
            span.last = hit && x > span.last ? x : span.last;
        }
    }
    return span;
}

typedef struct Box {
    int left;
    int top;
    int right;
    int bottom;
} Box;

static Box ink_box(const Color *grid, int layout, Rect area)
{
    int left = area.width;
    int top = area.height;
    int right = -1;
    int bottom = -1;
    for (int y = 0; y < area.height; y++) {
        for (int x = 0; x < area.width; x++) {
            bool hit = at(grid, layout, (Point){area.x + x, area.y + y}) == INK;
            left = hit && x < left ? x : left;
            top = hit && y < top ? y : top;
            right = hit && x > right ? x : right;
            bottom = hit && y > bottom ? y : bottom;
        }
    }
    return (Box){left, top, right, bottom};
}

static bool colon_dot_at(const Color *grid, int layout, Point from)
{
    bool edges = at(grid, layout, (Point){from.x - 1, from.y}) != INK &&
                 at(grid, layout, (Point){from.x + COLON_DOT_WIDTH, from.y}) != INK;
    for (int x = 0; x < COLON_DOT_WIDTH; x++) {
        edges = edges && at(grid, layout, (Point){from.x + x, from.y}) == INK;
    }
    return edges;
}

static void both_colons_sit_on_their_exact_columns(void)
{
    static const int UPPER[2] = {COLON_LEFT_UPPER, COLON_RIGHT_UPPER};
    static const int LOWER[2] = {COLON_LEFT_LOWER, COLON_RIGHT_LOWER};
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        Point big = spot_of(layout)->big;
        scene.frame.shown_ms = SHOWN_1_23_45;
        draw_into(&scene, pixels);
        for (int colon = 0; colon < 2; colon++) {
            Point upper = {big.x + UPPER[colon], big.y + COLON_UPPER_ROW};
            Point lower = {big.x + LOWER[colon], big.y + COLON_LOWER_ROW};
            CHECK(colon_dot_at(pixels, layout, upper));
            CHECK(colon_dot_at(pixels, layout, lower));
        }
    }
}

static void the_big_digits_fill_their_exact_box(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        Point big = spot_of(layout)->big;
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        scene.frame.shown_ms = SHOWN_8_08_08;
        draw_into(&scene, pixels);
        Box box = ink_box(pixels, layout, (Rect){big.x, big.y, BIG_BOX_WIDTH, BIG_BOX_HEIGHT});
        CHECK_EQUAL(0, box.left);
        CHECK_EQUAL(0, box.top);
        CHECK_EQUAL(BIG_LAST_COLUMN, box.right);
        CHECK_EQUAL(BIG_LAST_ROW, box.bottom);
    }
}

static void each_legend_starts_and_ends_on_its_exact_columns(void)
{
    static const Span SPANS[LEGEND_COUNT] = {{0, 12}, {1, 11}, {0, 12}, {0, 0}, {1, 6}};
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        for (int legend = 0; legend < LEGEND_COUNT; legend++) {
            NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
            Point corner = spot_of(layout)->legends[legend];
            scene.status.shuffle = SHUFFLE_ALBUMS;
            scene.status.repeat = legend == 3 ? REPEAT_ONE : REPEAT_ALL;
            draw_into(&scene, pixels);
            Span span =
                dark_span(pixels, layout, (Rect){corner.x, corner.y, LEGEND_SCAN, LEGEND_HEIGHT});
            CHECK_EQUAL(SPANS[legend].first, span.first);
            CHECK_EQUAL(SPANS[legend].last, span.last);
        }
    }
}

static void the_badge_icon_fills_its_exact_square(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        Point badge = spot_of(layout)->badge;
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        scene.frame.feedback = NOW_PLAYING_FEEDBACK_NEXT;
        draw_into(&scene, pixels);
        Box box = ink_box(pixels, layout, (Rect){badge.x - 3, badge.y - 3, 24, 24});
        CHECK_EQUAL(3, box.left);
        CHECK_EQUAL(3, box.top);
        CHECK_EQUAL(BADGE_SIDE + 2, box.right);
        CHECK_EQUAL(BADGE_SIDE + 2, box.bottom);
    }
}

static void the_portrait_artist_and_album_rows_show_their_own_text(void)
{
    for (int layout = 1; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        draw_into(&scene, pixels);
        scene.frame.album = "ZZZZ";
        draw_into(&scene, other);
        CHECK(differing_rows(layout) > 0);
        CHECK(rows_within(layout, LYRIC_PORT_Y, LYRIC_PORT_Y + TEXT_HEIGHT - 1));
        NpTestScene fresh = scene_of(layout, NP_TEST_PLAYING);
        draw_into(&fresh, pixels);
        fresh.frame.artist = "ZZZZ";
        draw_into(&fresh, other);
        CHECK(differing_rows(layout) > 0);
        CHECK(rows_within(layout, ARTIST_PORT_Y, ARTIST_PORT_Y + TEXT_HEIGHT - 1));
    }
}

static void idle_lights_neither_transport_legend(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        const Spot *spot = spot_of(layout);
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        scene.status.state = PLAYER_IDLE;
        scene.frame.playing = false;
        draw_into(&scene, pixels);
        CHECK_EQUAL(GHOST, at(pixels, layout, (Point){spot->play_x, 11}));
        CHECK_EQUAL(GHOST, at(pixels, layout, (Point){spot->pause_x, 7}));
    }
}

static void the_stream_text_stops_a_gap_before_the_remaining_time(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        Rect info = info_box(layout, 0, spot_of(layout)->info_width);
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        scene.frame.stream = "";
        draw_into(&scene, other);
        Span time = dark_span(other, layout, info);
        scene.frame.stream = NP_TEST_LONG_TITLE;
        draw_into(&scene, pixels);
        Rect gap = {info.x + time.first - INFO_GAP + 1, info.y, INFO_GAP - 1, info.height};
        CHECK_EQUAL(0, count_dark(pixels, layout, gap));
    }
}

static void the_cover_is_never_drawn(void)
{
    for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        NpTestScene scene = scene_of(layout, NP_TEST_PLAYING);
        draw_into(&scene, pixels);
        scene.frame.cover = (Image){0};
        draw_into(&scene, other);
        CHECK(np_test_same(pixels, other, NP_TEST_LAYOUTS[layout]));
        Rect area = NP_TEST_LAYOUTS[layout].area;
        CHECK_EQUAL(
            0, count_in(pixels, layout, (Rect){0, -TOP_INSET, area.width, area.height}, COVER_RED));
    }
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_glass_has_a_lip_and_cut_corners_in_a_bezel),
        TEST_CASE(the_play_and_pause_legends_follow_the_state),
        TEST_CASE(the_mode_legends_light_only_for_their_modes),
        TEST_CASE(the_big_digits_show_hours_minutes_and_seconds),
        TEST_CASE(leading_zeros_stay_ghosted_and_the_seconds_always_show),
        TEST_CASE(the_digits_follow_the_scrub_target_not_the_position),
        TEST_CASE(times_beyond_nine_hours_clamp_to_the_last_displayable_second),
        TEST_CASE(the_hours_colon_is_lit_only_with_hours),
        TEST_CASE(volume_shows_its_level_in_the_last_two_digits_with_both_colons_off),
        TEST_CASE(a_volume_beyond_ninety_nine_shows_ninety_nine),
        TEST_CASE(the_track_number_fills_the_medium_digits_without_leading_zeros),
        TEST_CASE(the_spectrum_levels_match_the_reference_hash),
        TEST_CASE(a_paused_spectrum_has_every_cell_off),
        TEST_CASE(a_paused_face_ignores_the_tick),
        TEST_CASE(the_next_tick_changes_only_spectrum_rows),
        TEST_CASE(a_badge_replaces_the_spectrum_with_an_icon),
        TEST_CASE(the_title_sits_in_a_dotted_field),
        TEST_CASE(the_title_text_is_drawn_in_ink_over_the_field),
        TEST_CASE(the_progress_cells_follow_the_shown_position),
        TEST_CASE(the_progress_cells_are_three_wide_with_a_gap),
        TEST_CASE(an_unknown_duration_lights_no_cell),
        TEST_CASE(volume_and_scrubbing_drive_the_cells),
        TEST_CASE(the_info_row_has_the_stream_left_and_the_remaining_time_right),
        TEST_CASE(volume_and_scrubbing_replace_the_whole_info_row),
        TEST_CASE(the_codec_box_hugs_its_text_and_hides_when_unknown),
        TEST_CASE(a_lyric_replaces_only_the_artist_or_album_row),
        TEST_CASE(both_colons_sit_on_their_exact_columns),
        TEST_CASE(the_big_digits_fill_their_exact_box),
        TEST_CASE(each_legend_starts_and_ends_on_its_exact_columns),
        TEST_CASE(the_badge_icon_fills_its_exact_square),
        TEST_CASE(the_portrait_artist_and_album_rows_show_their_own_text),
        TEST_CASE(idle_lights_neither_transport_legend),
        TEST_CASE(the_stream_text_stops_a_gap_before_the_remaining_time),
        TEST_CASE(the_cover_is_never_drawn),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

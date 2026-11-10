#include <string.h>

#include "check.h"
#include "core/i18n.h"
#include "face_text.h"
#include "np_faces_test.h"
#include "tiny_font.h"
#include "ui/label.h"

enum {
    WIDTH = 64,
    HEIGHT = 20,
    ICON = 18,
    BACKGROUND = 0,
    EMPHASIS = 0xFFFF,
    NORMAL = 0x07E0,
    HOT = 0xF800,
    TRACK = 0x2104
};

static const MarqueeTiming TIMING = {.pause_ms = 1200, .speed_px_per_s = 40, .gap_px = 32};
static const char LONG_TEXT[] = "ABABABABABABABABABABABAB";

static Color grid[WIDTH * HEIGHT];
static Color other[WIDTH * HEIGHT];

static Canvas fresh(Color *pixels)
{
    for (int index = 0; index < WIDTH * HEIGHT; index++) {
        pixels[index] = BACKGROUND;
    }
    return canvas_make(pixels, (Rect){0, 0, WIDTH, HEIGHT});
}

static bool same(void)
{
    return memcmp(grid, other, sizeof grid) == 0;
}

static int count_of(const Color *pixels, Color color)
{
    int count = 0;
    for (int index = 0; index < WIDTH * HEIGHT; index++) {
        count += pixels[index] == color ? 1 : 0;
    }
    return count;
}

static Color at(const Color *pixels, int x, int y)
{
    return pixels[y * WIDTH + x];
}

static NpTestScene scene_of(NpTestState state)
{
    return np_test_scene(NP_FACE_CASSETTE, NP_TEST_LAYOUTS[0], state);
}

static FaceLine line_of(FaceAlign align)
{
    return (FaceLine){.font = &TINY_FONT,
                      .area = {0, 2, WIDTH, 6},
                      .emphasis = EMPHASIS,
                      .color = NORMAL,
                      .align = align};
}

static void the_scroll_waits_for_the_pause_then_steps_four_pixels_per_tick(void)
{
    CHECK_EQUAL(0, face_scroll_offset(TIMING, 100, 50, 0));
    CHECK_EQUAL(0, face_scroll_offset(TIMING, 100, 50, 1199));
    CHECK_EQUAL(0, face_scroll_offset(TIMING, 100, 50, 1200));
    CHECK_EQUAL(4, face_scroll_offset(TIMING, 100, 50, 1300));
    CHECK_EQUAL(8, face_scroll_offset(TIMING, 100, 50, 1400));
    CHECK_EQUAL(12, face_scroll_offset(TIMING, 100, 50, 1500));
}

static void the_scroll_only_moves_on_whole_ticks(void)
{
    for (uint32_t elapsed = 1300; elapsed < 1400; elapsed++) {
        CHECK_EQUAL(4, face_scroll_offset(TIMING, 100, 50, elapsed));
    }
    CHECK_EQUAL(face_scroll_offset(TIMING, 100, 50, 2000),
                face_scroll_offset(TIMING, 100, 50, 2099));
}

static void the_scroll_runs_the_gap_and_starts_over(void)
{
    CHECK_EQUAL(0, face_scroll_offset(TIMING, 100, 50, 4500));
    CHECK_EQUAL(0, face_scroll_offset(TIMING, 100, 50, 4500 + 1100));
    CHECK_EQUAL(4, face_scroll_offset(TIMING, 100, 50, 4500 + 1300));
    CHECK_EQUAL(128, face_scroll_offset(TIMING, 100, 50, 4400));
}

static void text_that_fits_or_a_zero_speed_never_scrolls(void)
{
    CHECK_EQUAL(0, face_scroll_offset(TIMING, 50, 50, 3000));
    CHECK_EQUAL(0, face_scroll_offset(TIMING, 40, 50, 3000));
    MarqueeTiming still = {.pause_ms = 1200, .speed_px_per_s = 0, .gap_px = 32};
    CHECK_EQUAL(0, face_scroll_offset(still, 100, 50, 3000));
}

static void the_scroll_uses_the_time_since_the_text_changed(void)
{
    NpTestScene early = scene_of(NP_TEST_PLAYING);
    early.frame.text_ms = 1000;
    early.frame.now_ms = 1000 + 1500;
    NpTestScene late = scene_of(NP_TEST_PLAYING);
    late.frame.text_ms = 0;
    late.frame.now_ms = 1500;
    FaceLine line = line_of(FACE_ALIGN_LEFT);
    NowPlayingFrame early_frame = np_test_frame(&early);
    NowPlayingFrame late_frame = np_test_frame(&late);
    Canvas first = fresh(grid);
    Canvas second = fresh(other);
    face_line(&first, &early_frame, &line, LONG_TEXT);
    face_line(&second, &late_frame, &line, LONG_TEXT);
    CHECK(same());
}

static void a_playing_long_line_scrolls_between_ticks(void)
{
    NpTestScene scene = scene_of(NP_TEST_PLAYING);
    scene.frame.text_ms = 0;
    FaceLine line = line_of(FACE_ALIGN_LEFT);
    scene.frame.now_ms = 1000;
    NowPlayingFrame start = np_test_frame(&scene);
    Canvas first = fresh(grid);
    face_line(&first, &start, &line, LONG_TEXT);
    scene.frame.now_ms = 1500;
    NowPlayingFrame later = np_test_frame(&scene);
    Canvas second = fresh(other);
    face_line(&second, &later, &line, LONG_TEXT);
    CHECK(!same());
}

static void a_new_long_lyric_scrolls_from_its_own_start_not_the_tracks(void)
{
    NpTestScene scene = scene_of(NP_TEST_PLAYING);
    scene.frame.now_ms = 60000;
    scene.frame.lyric_ms = 60000;
    scene.frame.text_ms = 60000 - 47300;
    scene.frame.lyric = LONG_TEXT;
    NowPlayingFrame frame = np_test_frame(&scene);
    FaceLine line = line_of(FACE_ALIGN_LEFT);
    Canvas first = fresh(grid);
    face_line(&first, &frame, &line, frame.lyric);
    Canvas offset_zero = fresh(other);
    label_draw_scrolling(&offset_zero, (TextStyle){&TINY_FONT, NORMAL},
                         (Label){line.area, LONG_TEXT}, (Marquee){0, 32});
    CHECK(same());
}

static void a_paused_long_line_is_static_and_matches_a_plain_label(void)
{
    NpTestScene scene = scene_of(NP_TEST_PAUSED);
    FaceLine line = line_of(FACE_ALIGN_LEFT);
    scene.frame.now_ms = 1000;
    NowPlayingFrame start = np_test_frame(&scene);
    Canvas first = fresh(grid);
    face_line(&first, &start, &line, LONG_TEXT);
    scene.frame.now_ms = 9000;
    NowPlayingFrame later = np_test_frame(&scene);
    Canvas second = fresh(other);
    face_line(&second, &later, &line, LONG_TEXT);
    CHECK(same());
    Canvas plain = fresh(other);
    label_draw(&plain, (TextStyle){&TINY_FONT, NORMAL}, (Label){line.area, LONG_TEXT});
    CHECK(same());
    CHECK(count_of(grid, NORMAL) > 0);
}

static void a_scrubbing_long_line_is_static_even_while_playing(void)
{
    NpTestScene scene = scene_of(NP_TEST_SCRUBBING);
    scene.frame.playing = true;
    FaceLine line = line_of(FACE_ALIGN_LEFT);
    scene.frame.now_ms = 1500;
    NowPlayingFrame start = np_test_frame(&scene);
    Canvas first = fresh(grid);
    face_line(&first, &start, &line, LONG_TEXT);
    scene.frame.now_ms = 3000;
    NowPlayingFrame later = np_test_frame(&scene);
    Canvas second = fresh(other);
    face_line(&second, &later, &line, LONG_TEXT);
    CHECK(same());
}

static void a_null_line_draws_nothing(void)
{
    NpTestScene scene = scene_of(NP_TEST_PLAYING);
    NowPlayingFrame frame = np_test_frame(&scene);
    FaceLine line = line_of(FACE_ALIGN_LEFT);
    Canvas canvas = fresh(grid);
    face_line(&canvas, &frame, &line, NULL);
    CHECK_EQUAL(WIDTH * HEIGHT, count_of(grid, BACKGROUND));
}

static void text_that_fits_is_placed_by_its_alignment(void)
{
    Rect area = {10, 2, 40, 6};
    int width = text_width(&TINY_FONT, "AB");
    Rect left = face_fitted_area(&TINY_FONT, area, "AB", FACE_ALIGN_LEFT);
    Rect centre = face_fitted_area(&TINY_FONT, area, "AB", FACE_ALIGN_CENTRE);
    Rect right = face_fitted_area(&TINY_FONT, area, "AB", FACE_ALIGN_RIGHT);
    CHECK_EQUAL(10, left.x);
    CHECK_EQUAL(40, left.width);
    CHECK_EQUAL(10 + (40 - width) / 2, centre.x);
    CHECK_EQUAL(width, centre.width);
    CHECK_EQUAL(10 + 40 - width, right.x);
    CHECK_EQUAL(width, right.width);
}

static void text_wider_than_its_area_is_capped_to_the_area(void)
{
    Rect area = {10, 2, 20, 6};
    Rect centre = face_fitted_area(&TINY_FONT, area, LONG_TEXT, FACE_ALIGN_CENTRE);
    Rect right = face_fitted_area(&TINY_FONT, area, LONG_TEXT, FACE_ALIGN_RIGHT);
    CHECK_EQUAL(10, centre.x);
    CHECK_EQUAL(20, centre.width);
    CHECK_EQUAL(10, right.x);
}

static void a_centred_line_that_fits_matches_a_label_in_the_fitted_area(void)
{
    NpTestScene scene = scene_of(NP_TEST_PLAYING);
    NowPlayingFrame frame = np_test_frame(&scene);
    FaceLine line = line_of(FACE_ALIGN_CENTRE);
    Canvas first = fresh(grid);
    face_line(&first, &frame, &line, "AB");
    Rect fitted = face_fitted_area(&TINY_FONT, line.area, "AB", FACE_ALIGN_CENTRE);
    Canvas second = fresh(other);
    label_draw(&second, (TextStyle){&TINY_FONT, NORMAL}, (Label){fitted, "AB"});
    CHECK(same());
    CHECK(count_of(grid, NORMAL) > 0);
}

static void the_times_are_elapsed_and_minus_remaining(void)
{
    NpTestScene scene = scene_of(NP_TEST_PLAYING);
    NowPlayingFrame frame = np_test_frame(&scene);
    FaceTimes times;
    face_times_text(&frame, &times);
    CHECK(strcmp(times.elapsed, "2:19") == 0);
    CHECK(strcmp(times.remaining, "-1:45") == 0);
}

static void the_times_follow_the_scrub_target(void)
{
    NpTestScene scene = scene_of(NP_TEST_SCRUBBING);
    NowPlayingFrame frame = np_test_frame(&scene);
    FaceTimes times;
    face_times_text(&frame, &times);
    CHECK(strcmp(times.elapsed, "3:21") == 0);
    CHECK(strcmp(times.remaining, "-0:44") == 0);
}

static void an_unknown_duration_or_an_overrun_shows_zero_remaining(void)
{
    NpTestScene scene = scene_of(NP_TEST_PLAYING);
    scene.status.duration_ms = 0;
    NowPlayingFrame frame = np_test_frame(&scene);
    FaceTimes times;
    face_times_text(&frame, &times);
    CHECK(strcmp(times.remaining, "-0:00") == 0);
    scene.status.duration_ms = 1000;
    frame = np_test_frame(&scene);
    face_times_text(&frame, &times);
    CHECK(strcmp(times.remaining, "-0:00") == 0);
}

static void ten_hours_and_more_fit_the_time_buffers(void)
{
    NpTestScene scene = scene_of(NP_TEST_PLAYING);
    scene.status.duration_ms = 72000000;
    scene.frame.shown_ms = 36000000;
    NowPlayingFrame frame = np_test_frame(&scene);
    FaceTimes times;
    face_times_text(&frame, &times);
    CHECK(strcmp(times.elapsed, "10:00:00") == 0);
    CHECK(strcmp(times.remaining, "-10:00:00") == 0);
    scene.frame.shown_ms = 0;
    frame = np_test_frame(&scene);
    face_times_text(&frame, &times);
    CHECK(strcmp(times.remaining, "-20:00:00") == 0);
}

static void the_times_are_drawn_elapsed_left_and_remaining_right(void)
{
    NpTestScene scene = scene_of(NP_TEST_PLAYING);
    NowPlayingFrame frame = np_test_frame(&scene);
    FaceLine line = line_of(FACE_ALIGN_LEFT);
    Canvas canvas = fresh(grid);
    face_times_draw(&canvas, &frame, &line);
    FaceTimes times;
    face_times_text(&frame, &times);
    Canvas expected = fresh(other);
    label_draw(&expected, (TextStyle){&TINY_FONT, EMPHASIS}, (Label){line.area, times.elapsed});
    Rect right = face_fitted_area(&TINY_FONT, line.area, times.remaining, FACE_ALIGN_RIGHT);
    label_draw(&expected, (TextStyle){&TINY_FONT, NORMAL}, (Label){right, times.remaining});
    CHECK(same());
    CHECK(count_of(grid, BACKGROUND) < WIDTH * HEIGHT);
}

static void no_feedback_means_no_feedback_text(void)
{
    NpTestScene scene = scene_of(NP_TEST_PLAYING);
    NowPlayingFrame frame = np_test_frame(&scene);
    char buffer[FACE_FEEDBACK_CAPACITY];
    CHECK(face_feedback_text(&frame, buffer, sizeof buffer) == NULL);
}

static void the_volume_feedback_names_the_level(void)
{
    NpTestScene scene = scene_of(NP_TEST_VOLUME);
    NowPlayingFrame frame = np_test_frame(&scene);
    char buffer[FACE_FEEDBACK_CAPACITY];
    const char *text = face_feedback_text(&frame, buffer, sizeof buffer);
    CHECK(text != NULL);
    CHECK(strcmp(text, "Volume 32") == 0);
}

static void scrubbing_beats_the_volume_feedback(void)
{
    NpTestScene scene = scene_of(NP_TEST_VOLUME);
    scene.frame.scrubbing = true;
    NowPlayingFrame frame = np_test_frame(&scene);
    char buffer[FACE_FEEDBACK_CAPACITY];
    const char *text = face_feedback_text(&frame, buffer, sizeof buffer);
    CHECK(strcmp(text, i18n_text(STR_MENU_SEEKING_HINT)) == 0);
}

static void a_small_buffer_cuts_the_feedback_text_short_without_overflowing(void)
{
    NpTestScene scene = scene_of(NP_TEST_VOLUME);
    NowPlayingFrame frame = np_test_frame(&scene);
    char buffer[4];
    const char *text = face_feedback_text(&frame, buffer, sizeof buffer);
    CHECK(strlen(text) < sizeof buffer);
}

static void draw_feedback_line(const NpTestScene *scene, Color *pixels)
{
    NowPlayingFrame frame = np_test_frame(scene);
    FaceLine line = line_of(FACE_ALIGN_LEFT);
    Canvas canvas = fresh(pixels);
    face_feedback_line(&canvas, &frame, &line, "BA");
}

static void the_normal_text_shows_in_the_secondary_colour_when_nothing_overrides_it(void)
{
    NpTestScene scene = scene_of(NP_TEST_PLAYING);
    draw_feedback_line(&scene, grid);
    NowPlayingFrame frame = np_test_frame(&scene);
    FaceLine line = line_of(FACE_ALIGN_LEFT);
    Canvas expected = fresh(other);
    face_line(&expected, &frame, &line, "BA");
    CHECK(same());
    CHECK_EQUAL(0, count_of(grid, EMPHASIS));
}

static void a_lyric_replaces_the_text_in_the_emphasis_colour(void)
{
    NpTestScene scene = scene_of(NP_TEST_LYRIC);
    draw_feedback_line(&scene, grid);
    NowPlayingFrame frame = np_test_frame(&scene);
    FaceLine line = line_of(FACE_ALIGN_LEFT);
    line.color = EMPHASIS;
    Canvas expected = fresh(other);
    face_line(&expected, &frame, &line, "AB");
    CHECK(same());
    CHECK_EQUAL(0, count_of(grid, NORMAL));
}

static void an_empty_lyric_falls_back_to_the_text(void)
{
    NpTestScene scene = scene_of(NP_TEST_LYRIC);
    scene.frame.lyric = NULL;
    draw_feedback_line(&scene, grid);
    NpTestScene plain = scene_of(NP_TEST_PLAYING);
    draw_feedback_line(&plain, other);
    CHECK(same());
}

static void the_volume_feedback_replaces_the_lyric_as_a_static_label(void)
{
    NpTestScene scene = scene_of(NP_TEST_VOLUME);
    scene.frame.lyric = "AB";
    draw_feedback_line(&scene, grid);
    FaceLine line = line_of(FACE_ALIGN_LEFT);
    Canvas expected = fresh(other);
    label_draw(&expected, (TextStyle){&TINY_FONT, EMPHASIS}, (Label){line.area, "Volume 32"});
    CHECK(same());
}

static void scrubbing_replaces_everything_with_the_hint(void)
{
    NpTestScene scene = scene_of(NP_TEST_SCRUBBING);
    scene.frame.lyric = "AB";
    scene.frame.volume_shown = true;
    draw_feedback_line(&scene, grid);
    FaceLine line = line_of(FACE_ALIGN_LEFT);
    const char *hint = i18n_text(STR_MENU_SEEKING_HINT);
    Canvas expected = fresh(other);
    label_draw(&expected, (TextStyle){&TINY_FONT, EMPHASIS}, (Label){line.area, hint});
    CHECK(same());
}

static void the_progress_width_is_proportional_and_clamped(void)
{
    CHECK_EQUAL(0, face_progress_width(1000, 0, 170));
    CHECK_EQUAL(0, face_progress_width(0, 245000, 170));
    CHECK_EQUAL(96, face_progress_width(139600, 245000, 170));
    CHECK_EQUAL(170, face_progress_width(245000, 245000, 170));
    CHECK_EQUAL(170, face_progress_width(999999, 245000, 170));
    CHECK_EQUAL(750, face_progress_width(3000000000u, 4000000000u, 1000));
}

static void the_volume_width_scales_to_fifty_and_clamps(void)
{
    CHECK_EQUAL(0, face_volume_width(0, 150));
    CHECK_EQUAL(96, face_volume_width(32, 150));
    CHECK_EQUAL(75, face_volume_width(25, 150));
    CHECK_EQUAL(150, face_volume_width(50, 150));
    CHECK_EQUAL(150, face_volume_width(200, 150));
}

static void the_bar_shows_the_position_in_the_normal_colour(void)
{
    NpTestScene scene = scene_of(NP_TEST_PLAYING);
    NowPlayingFrame frame = np_test_frame(&scene);
    FaceFill fill = face_bar_fill(&frame, 170);
    CHECK_EQUAL(96, fill.width);
    CHECK(!fill.hot);
}

static void the_bar_follows_the_scrub_target_in_the_hot_colour(void)
{
    NpTestScene scene = scene_of(NP_TEST_SCRUBBING);
    NowPlayingFrame frame = np_test_frame(&scene);
    FaceFill fill = face_bar_fill(&frame, 170);
    CHECK_EQUAL(face_progress_width(201000, 245000, 170), fill.width);
    CHECK(fill.hot);
}

static void the_bar_shows_the_volume_in_the_hot_colour_even_while_scrubbing(void)
{
    NpTestScene scene = scene_of(NP_TEST_VOLUME);
    scene.frame.scrubbing = true;
    NowPlayingFrame frame = np_test_frame(&scene);
    FaceFill fill = face_bar_fill(&frame, 150);
    CHECK_EQUAL(96, fill.width);
    CHECK(fill.hot);
}

static void the_bar_draws_a_track_and_a_fill(void)
{
    NpTestScene scene = scene_of(NP_TEST_SCRUBBING);
    NowPlayingFrame frame = np_test_frame(&scene);
    FaceBar bar = {{4, 3, 50, 3}, TRACK, NORMAL, HOT};
    Canvas canvas = fresh(grid);
    face_bar_draw(&canvas, &frame, &bar);
    int filled = face_progress_width(201000, 245000, 50);
    CHECK_EQUAL(filled * 3, count_of(grid, HOT));
    CHECK_EQUAL((50 - filled) * 3, count_of(grid, TRACK));
    CHECK_EQUAL(HOT, at(grid, 4, 3));
    CHECK_EQUAL(TRACK, at(grid, 4 + filled, 3));
    CHECK_EQUAL(BACKGROUND, at(grid, 3, 3));
    CHECK_EQUAL(BACKGROUND, at(grid, 4, 6));
}

static void a_resting_bar_uses_the_normal_colour(void)
{
    NpTestScene scene = scene_of(NP_TEST_PLAYING);
    NowPlayingFrame frame = np_test_frame(&scene);
    FaceBar bar = {{4, 3, 50, 3}, TRACK, NORMAL, HOT};
    Canvas canvas = fresh(grid);
    face_bar_draw(&canvas, &frame, &bar);
    CHECK_EQUAL(0, count_of(grid, HOT));
    CHECK_EQUAL(face_progress_width(139600, 245000, 50) * 3, count_of(grid, NORMAL));
}

static void the_badge_kind_maps_each_feedback(void)
{
    static const struct {
        NowPlayingFeedback feedback;
        FaceIcon icon;
    } MAP[] = {
        {NOW_PLAYING_FEEDBACK_NONE, FACE_ICON_NONE},
        {NOW_PLAYING_FEEDBACK_VOLUME, FACE_ICON_NONE},
        {NOW_PLAYING_FEEDBACK_NEXT, FACE_ICON_NEXT},
        {NOW_PLAYING_FEEDBACK_PREVIOUS, FACE_ICON_PREVIOUS},
        {NOW_PLAYING_FEEDBACK_PAUSE, FACE_ICON_PAUSE},
        {NOW_PLAYING_FEEDBACK_PLAY, FACE_ICON_PLAY},
        {(NowPlayingFeedback)99, FACE_ICON_NONE},
    };
    NpTestScene scene = scene_of(NP_TEST_PLAYING);
    for (size_t index = 0; index < sizeof MAP / sizeof MAP[0]; index++) {
        scene.frame.feedback = MAP[index].feedback;
        NowPlayingFrame frame = np_test_frame(&scene);
        CHECK_EQUAL(MAP[index].icon, face_badge_kind(&frame));
    }
}

static int icon_pixels(FaceIcon icon)
{
    Canvas canvas = fresh(grid);
    face_icon_draw(&canvas, icon, (Rect){2, 0, ICON, ICON}, HOT);
    return count_of(grid, HOT);
}

static void the_icons_cover_the_pixels_of_their_shape(void)
{
    CHECK_EQUAL(0, icon_pixels(FACE_ICON_NONE));
    CHECK_EQUAL(162, icon_pixels(FACE_ICON_PLAY));
    CHECK_EQUAL(180, icon_pixels(FACE_ICON_PAUSE));
    CHECK_EQUAL(258, icon_pixels(FACE_ICON_NEXT));
    CHECK_EQUAL(258, icon_pixels(FACE_ICON_PREVIOUS));
}

static void next_has_its_bar_on_the_right_and_previous_on_the_left(void)
{
    Canvas next = fresh(grid);
    face_icon_draw(&next, FACE_ICON_NEXT, (Rect){2, 0, ICON, ICON}, HOT);
    Canvas previous = fresh(other);
    face_icon_draw(&previous, FACE_ICON_PREVIOUS, (Rect){2, 0, ICON, ICON}, HOT);
    CHECK_EQUAL(HOT, at(grid, 2 + ICON - 1, 0));
    CHECK_EQUAL(BACKGROUND, at(grid, 3, 0));
    CHECK_EQUAL(HOT, at(other, 3, 0));
    CHECK_EQUAL(BACKGROUND, at(other, 2 + ICON - 1, 0));
}

static void the_badge_draws_the_icon_of_the_feedback_and_nothing_otherwise(void)
{
    NpTestScene scene = scene_of(NP_TEST_BADGE);
    NowPlayingFrame frame = np_test_frame(&scene);
    Canvas canvas = fresh(grid);
    face_badge_draw(&canvas, &frame, (Rect){2, 0, ICON, ICON}, HOT);
    CHECK_EQUAL(258, count_of(grid, HOT));
    scene.frame.feedback = NOW_PLAYING_FEEDBACK_VOLUME;
    frame = np_test_frame(&scene);
    Canvas empty = fresh(other);
    face_badge_draw(&empty, &frame, (Rect){2, 0, ICON, ICON}, HOT);
    CHECK_EQUAL(0, count_of(other, HOT));
}

static void an_outline_draws_four_edges_and_keeps_the_inside(void)
{
    Canvas canvas = fresh(grid);
    face_outline(&canvas, (Rect){3, 2, 10, 6}, HOT);
    CHECK_EQUAL(2 * 10 + 2 * 4, count_of(grid, HOT));
    CHECK_EQUAL(HOT, at(grid, 3, 2));
    CHECK_EQUAL(HOT, at(grid, 12, 7));
    CHECK_EQUAL(BACKGROUND, at(grid, 5, 4));
}

static void the_codec_box_is_the_text_plus_eight_pixels_and_hidden_when_unknown(void)
{
    CHECK_EQUAL(text_width(&TINY_FONT, "AB") + 8, face_codec_width(&TINY_FONT, "AB"));
    CHECK_EQUAL(0, face_codec_width(&TINY_FONT, ""));
    CHECK_EQUAL(0, face_codec_width(&TINY_FONT, NULL));
}

static void the_codec_box_outlines_its_area_and_centres_the_name(void)
{
    FaceLine box = line_of(FACE_ALIGN_LEFT);
    box.area = (Rect){10, 1, 15, 8};
    Canvas canvas = fresh(grid);
    face_codec_draw(&canvas, &box, "AB");
    CHECK_EQUAL(NORMAL, at(grid, 10, 1));
    CHECK_EQUAL(NORMAL, at(grid, 24, 8));
    Canvas hidden = fresh(other);
    face_codec_draw(&hidden, &box, "");
    CHECK_EQUAL(WIDTH * HEIGHT, count_of(other, BACKGROUND));
}

static void the_format_row_puts_the_box_first_when_left_aligned(void)
{
    NpTestScene scene = scene_of(NP_TEST_PLAYING);
    scene.frame.codec = "A";
    scene.frame.stream = "AB";
    NowPlayingFrame frame = np_test_frame(&scene);
    FaceLine line = line_of(FACE_ALIGN_LEFT);
    line.area = (Rect){0, 0, WIDTH, 10};
    Canvas canvas = fresh(grid);
    face_format_row(&canvas, &frame, &line);
    CHECK_EQUAL(NORMAL, at(grid, 0, 0));
    int box = face_codec_width(&TINY_FONT, "A");
    CHECK_EQUAL(NORMAL, at(grid, box - 1, 0));
    CHECK_EQUAL(BACKGROUND, at(grid, box, 0));
    CHECK(count_of(grid, NORMAL) > 2 * box + 2 * 8);
}

static void the_format_row_centres_the_box_and_stream_together(void)
{
    NpTestScene scene = scene_of(NP_TEST_PLAYING);
    scene.frame.codec = "A";
    scene.frame.stream = "AB";
    NowPlayingFrame frame = np_test_frame(&scene);
    FaceLine line = line_of(FACE_ALIGN_CENTRE);
    line.area = (Rect){0, 0, WIDTH, 10};
    Canvas canvas = fresh(grid);
    face_format_row(&canvas, &frame, &line);
    int total = face_codec_width(&TINY_FONT, "A") + 7 + text_width(&TINY_FONT, "AB");
    int origin = (WIDTH - total) / 2;
    CHECK_EQUAL(BACKGROUND, at(grid, origin - 1, 0));
    CHECK_EQUAL(NORMAL, at(grid, origin, 0));
}

static void the_format_row_without_a_codec_has_no_box(void)
{
    NpTestScene scene = scene_of(NP_TEST_PLAYING);
    scene.frame.codec = NULL;
    scene.frame.stream = "AB";
    NowPlayingFrame frame = np_test_frame(&scene);
    FaceLine line = line_of(FACE_ALIGN_LEFT);
    line.area = (Rect){0, 0, WIDTH, 10};
    Canvas canvas = fresh(grid);
    face_format_row(&canvas, &frame, &line);
    CHECK_EQUAL(BACKGROUND, at(grid, 0, 0));
    Canvas expected = fresh(other);
    label_draw(&expected, (TextStyle){&TINY_FONT, NORMAL}, (Label){line.area, "AB"});
    CHECK(same());
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_scroll_waits_for_the_pause_then_steps_four_pixels_per_tick),
        TEST_CASE(the_scroll_only_moves_on_whole_ticks),
        TEST_CASE(the_scroll_runs_the_gap_and_starts_over),
        TEST_CASE(text_that_fits_or_a_zero_speed_never_scrolls),
        TEST_CASE(the_scroll_uses_the_time_since_the_text_changed),
        TEST_CASE(a_playing_long_line_scrolls_between_ticks),
        TEST_CASE(a_new_long_lyric_scrolls_from_its_own_start_not_the_tracks),
        TEST_CASE(a_paused_long_line_is_static_and_matches_a_plain_label),
        TEST_CASE(a_scrubbing_long_line_is_static_even_while_playing),
        TEST_CASE(a_null_line_draws_nothing),
        TEST_CASE(text_that_fits_is_placed_by_its_alignment),
        TEST_CASE(text_wider_than_its_area_is_capped_to_the_area),
        TEST_CASE(a_centred_line_that_fits_matches_a_label_in_the_fitted_area),
        TEST_CASE(the_times_are_elapsed_and_minus_remaining),
        TEST_CASE(the_times_follow_the_scrub_target),
        TEST_CASE(an_unknown_duration_or_an_overrun_shows_zero_remaining),
        TEST_CASE(ten_hours_and_more_fit_the_time_buffers),
        TEST_CASE(the_times_are_drawn_elapsed_left_and_remaining_right),
        TEST_CASE(no_feedback_means_no_feedback_text),
        TEST_CASE(the_volume_feedback_names_the_level),
        TEST_CASE(scrubbing_beats_the_volume_feedback),
        TEST_CASE(a_small_buffer_cuts_the_feedback_text_short_without_overflowing),
        TEST_CASE(the_normal_text_shows_in_the_secondary_colour_when_nothing_overrides_it),
        TEST_CASE(a_lyric_replaces_the_text_in_the_emphasis_colour),
        TEST_CASE(an_empty_lyric_falls_back_to_the_text),
        TEST_CASE(the_volume_feedback_replaces_the_lyric_as_a_static_label),
        TEST_CASE(scrubbing_replaces_everything_with_the_hint),
        TEST_CASE(the_progress_width_is_proportional_and_clamped),
        TEST_CASE(the_volume_width_scales_to_fifty_and_clamps),
        TEST_CASE(the_bar_shows_the_position_in_the_normal_colour),
        TEST_CASE(the_bar_follows_the_scrub_target_in_the_hot_colour),
        TEST_CASE(the_bar_shows_the_volume_in_the_hot_colour_even_while_scrubbing),
        TEST_CASE(the_bar_draws_a_track_and_a_fill),
        TEST_CASE(a_resting_bar_uses_the_normal_colour),
        TEST_CASE(the_badge_kind_maps_each_feedback),
        TEST_CASE(the_icons_cover_the_pixels_of_their_shape),
        TEST_CASE(next_has_its_bar_on_the_right_and_previous_on_the_left),
        TEST_CASE(the_badge_draws_the_icon_of_the_feedback_and_nothing_otherwise),
        TEST_CASE(an_outline_draws_four_edges_and_keeps_the_inside),
        TEST_CASE(the_codec_box_is_the_text_plus_eight_pixels_and_hidden_when_unknown),
        TEST_CASE(the_codec_box_outlines_its_area_and_centres_the_name),
        TEST_CASE(the_format_row_puts_the_box_first_when_left_aligned),
        TEST_CASE(the_format_row_centres_the_box_and_stream_together),
        TEST_CASE(the_format_row_without_a_codec_has_no_box),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

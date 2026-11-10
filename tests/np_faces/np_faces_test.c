#include "np_faces_test.h"

#include <stdio.h>
#include <string.h>

#include "tiny_font.h"
#include "ui_test_theme.h"

enum {
    COVER_SIDE = 8,
    POSITION_MS = 139600,
    DURATION_MS = 245000,
    SCRUB_MS = 201000,
    VOLUME = 32,
    TEXT_MS = 60000,
    NOW_MS = 60600,
    TICK_MS = 100
};

const NpTestLayout NP_TEST_LAYOUTS[NP_TEST_LAYOUT_COUNT] = {
    {320, 170, {0, 24, 320, 146}},
    {170, 320, {0, 40, 170, 280}},
    {170, 320, {0, 46, 170, 274}},
};

const char NP_TEST_LONG_TITLE[] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                                  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                                  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                                  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

static const char *const TITLES[NP_TEST_TITLE_COUNT] = {
    "Midnight Drive at the Edge of Town",
    "",
    NP_TEST_LONG_TITLE,
};

static Color cover_pixels[COVER_SIDE * COVER_SIDE];

static Image cover_image(void)
{
    for (int index = 0; index < COVER_SIDE * COVER_SIDE; index++) {
        cover_pixels[index] = 0xF800;
    }
    return (Image){cover_pixels, COVER_SIDE, COVER_SIDE};
}

static Theme scene_theme(void)
{
    Theme theme = TEST_THEME;
    theme.fonts = (ThemeFonts){&TINY_FONT, &TINY_FONT, &TINY_FONT, &TINY_FONT};
    theme.marquee = (MarqueeTiming){.pause_ms = 1200, .speed_px_per_s = 40, .gap_px = 32};
    return theme;
}

static void apply_state(NpTestScene *scene, NpTestState state)
{
    NowPlayingFrame *frame = &scene->frame;
    bool paused = state == NP_TEST_PAUSED || state == NP_TEST_SCRUBBING || state == NP_TEST_VOLUME;
    frame->playing = !paused;
    scene->status.state = paused ? PLAYER_PAUSED : PLAYER_PLAYING;
    frame->scrubbing = state == NP_TEST_SCRUBBING;
    frame->shown_ms = state == NP_TEST_SCRUBBING ? SCRUB_MS : POSITION_MS;
    frame->volume_shown = state == NP_TEST_VOLUME;
    if (state == NP_TEST_VOLUME) {
        frame->feedback = NOW_PLAYING_FEEDBACK_VOLUME;
    }
    if (state == NP_TEST_BADGE) {
        frame->feedback = NOW_PLAYING_FEEDBACK_NEXT;
    }
    if (state == NP_TEST_LYRIC) {
        frame->lyric = "AB";
    }
    if (state == NP_TEST_NO_COVER) {
        frame->cover = (Image){0};
    }
}

NpTestScene np_test_scene(uint8_t face, NpTestLayout layout, NpTestState state)
{
    NpTestScene scene = {.theme = scene_theme(), .layout = layout};
    scene.status = (PlayerStatus){.state = PLAYER_PLAYING,
                                  .has_track = true,
                                  .position_ms = POSITION_MS,
                                  .duration_ms = DURATION_MS,
                                  .volume = VOLUME};
    scene.frame = (NowPlayingFrame){.song = TITLES[0],
                                    .artist = "The Long Way Home",
                                    .album = "Analog Hours",
                                    .codec = "FLAC",
                                    .stream = "24-bit 96 kHz",
                                    .detail = "2,116 kbps",
                                    .modes = "Shuffle songs",
                                    .cover = cover_image(),
                                    .area = layout.area,
                                    .now_ms = NOW_MS,
                                    .text_ms = TEXT_MS,
                                    .shown_ms = POSITION_MS,
                                    .tick = POSITION_MS / TICK_MS,
                                    .face = face,
                                    .playing = true};
    apply_state(&scene, state);
    return scene;
}

NowPlayingFrame np_test_frame(const NpTestScene *scene)
{
    NowPlayingFrame frame = scene->frame;
    frame.theme = &scene->theme;
    frame.status = &scene->status;
    return frame;
}

void np_test_clear(Color *pixels)
{
    for (int index = 0; index < NP_TEST_PIXELS; index++) {
        pixels[index] = NP_TEST_SENTINEL;
    }
}

void np_test_draw(const NpTestScene *scene, Color *pixels)
{
    NpTestLayout layout = scene->layout;
    np_test_clear(pixels);
    Canvas canvas = canvas_make(pixels, (Rect){0, 0, layout.width, layout.height});
    NowPlayingFrame frame = np_test_frame(scene);
    NOW_PLAYING_FACES.draw(&frame, &canvas);
}

static void draw_strip(const NpTestScene *scene, Color *pixels, int top)
{
    static Color strip[NP_TEST_MAX_WIDTH * NP_TEST_STRIP_ROWS];
    NpTestLayout layout = scene->layout;
    int rows = layout.height - top < NP_TEST_STRIP_ROWS ? layout.height - top : NP_TEST_STRIP_ROWS;
    for (int index = 0; index < layout.width * rows; index++) {
        strip[index] = NP_TEST_SENTINEL;
    }
    Canvas canvas = canvas_make(strip, (Rect){0, top, layout.width, rows});
    NowPlayingFrame frame = np_test_frame(scene);
    NOW_PLAYING_FACES.draw(&frame, &canvas);
    memcpy(pixels + top * layout.width, strip, (size_t)(layout.width * rows) * sizeof strip[0]);
}

void np_test_draw_strips(const NpTestScene *scene, Color *pixels)
{
    for (int top = 0; top < scene->layout.height; top += NP_TEST_STRIP_ROWS) {
        draw_strip(scene, pixels, top);
    }
}

bool np_test_same(const Color *first, const Color *second, NpTestLayout layout)
{
    return memcmp(first, second, (size_t)(layout.width * layout.height) * sizeof first[0]) == 0;
}

Color np_test_pixel(const Color *pixels, NpTestLayout layout, int x, int y)
{
    return pixels[y * layout.width + x];
}

bool np_test_area_painted(const Color *pixels, NpTestLayout layout)
{
    for (int y = layout.area.y; y < layout.area.y + layout.area.height; y++) {
        for (int x = layout.area.x; x < layout.area.x + layout.area.width; x++) {
            if (np_test_pixel(pixels, layout, x, y) == NP_TEST_SENTINEL) {
                return false;
            }
        }
    }
    return true;
}

bool np_test_outside_untouched(const Color *pixels, NpTestLayout layout)
{
    for (int y = 0; y < layout.height; y++) {
        bool inside_rows = y >= layout.area.y && y < layout.area.y + layout.area.height;
        for (int x = 0; x < layout.width; x++) {
            bool inside =
                inside_rows && x >= layout.area.x && x < layout.area.x + layout.area.width;
            if (!inside && np_test_pixel(pixels, layout, x, y) != NP_TEST_SENTINEL) {
                return false;
            }
        }
    }
    return true;
}

bool np_test_for_each_scene(NpTestCheck check)
{
    bool all = true;
    for (int face = NP_FACE_CASSETTE; face < NP_FACE_COUNT; face++) {
        for (int layout = 0; layout < NP_TEST_LAYOUT_COUNT; layout++) {
            for (int state = 0; state < NP_TEST_STATE_COUNT; state++) {
                for (int title = 0; title < NP_TEST_TITLE_COUNT; title++) {
                    NpTestScene scene =
                        np_test_scene((uint8_t)face, NP_TEST_LAYOUTS[layout], (NpTestState)state);
                    scene.frame.song = TITLES[title];
                    if (!check(&scene)) {
                        fprintf(stderr, "face %d layout %d state %d title %d\n", face, layout,
                                state, title);
                        all = false;
                    }
                }
            }
        }
    }
    return all;
}

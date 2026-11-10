#pragma once

#include <stdbool.h>

#include "np_faces/now_playing_face.h"

extern const NowPlayingFaces NOW_PLAYING_FACES;

enum {
    NP_TEST_MAX_WIDTH = 320,
    NP_TEST_MAX_HEIGHT = 170,
    NP_TEST_PIXELS = NP_TEST_MAX_WIDTH * NP_TEST_MAX_HEIGHT,
    NP_TEST_STRIP_ROWS = 34,
    NP_TEST_SENTINEL = 0x0821,
    NP_TEST_LAYOUT_COUNT = 3,
    NP_TEST_STATE_COUNT = 7,
    NP_TEST_TITLE_COUNT = 3,
    NP_TEST_PAUSED_SKEW_MS = 7300
};

typedef enum NpTestState {
    NP_TEST_PLAYING,
    NP_TEST_PAUSED,
    NP_TEST_SCRUBBING,
    NP_TEST_VOLUME,
    NP_TEST_BADGE,
    NP_TEST_LYRIC,
    NP_TEST_NO_COVER
} NpTestState;

typedef struct NpTestLayout {
    int width;
    int height;
    Rect area;
} NpTestLayout;

typedef struct NpTestScene {
    NowPlayingFrame frame;
    PlayerStatus status;
    Theme theme;
    NpTestLayout layout;
} NpTestScene;

typedef bool (*NpTestCheck)(const NpTestScene *scene);

extern const NpTestLayout NP_TEST_LAYOUTS[NP_TEST_LAYOUT_COUNT];
extern const char NP_TEST_LONG_TITLE[];

NpTestScene np_test_scene(uint8_t face, NpTestLayout layout, NpTestState state);
NowPlayingFrame np_test_frame(const NpTestScene *scene);
void np_test_clear(Color *pixels);
void np_test_draw(const NpTestScene *scene, Color *pixels);
void np_test_draw_strips(const NpTestScene *scene, Color *pixels);
bool np_test_same(const Color *first, const Color *second, NpTestLayout layout);
bool np_test_area_painted(const Color *pixels, NpTestLayout layout);
bool np_test_outside_untouched(const Color *pixels, NpTestLayout layout);
Color np_test_pixel(const Color *pixels, NpTestLayout layout, int x, int y);
bool np_test_for_each_scene(NpTestCheck check);

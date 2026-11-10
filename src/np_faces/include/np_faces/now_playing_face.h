#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/canvas.h"
#include "gfx/image.h"
#include "gfx/rect.h"
#include "playback/player_status.h"
#include "settings/settings.h"
#include "ui/theme.h"

enum {
    NP_FACE_TICK_MS = 100,
    NP_FACES_MAGIC = 0x46504E45u
};

typedef enum NowPlayingFeedback {
    NOW_PLAYING_FEEDBACK_NONE,
    NOW_PLAYING_FEEDBACK_VOLUME,
    NOW_PLAYING_FEEDBACK_NEXT,
    NOW_PLAYING_FEEDBACK_PREVIOUS,
    NOW_PLAYING_FEEDBACK_PAUSE,
    NOW_PLAYING_FEEDBACK_PLAY
} NowPlayingFeedback;

typedef struct NowPlayingFrame {
    const Theme *theme;
    const PlayerStatus *status;
    const char *song;
    const char *artist;
    const char *album;
    const char *lyric;
    const char *codec;
    const char *stream;
    const char *detail;
    const char *modes;
    Image cover;
    Rect area;
    uint32_t now_ms;
    uint32_t text_ms;
    uint32_t lyric_ms;
    uint32_t shown_ms;
    uint32_t tick;
    NowPlayingFeedback feedback;
    uint8_t face;
    bool playing;
    bool scrubbing;
    bool volume_shown;
} NowPlayingFrame;

typedef struct NowPlayingFaces {
    uint32_t magic;
    void (*draw)(const NowPlayingFrame *frame, Canvas *canvas);
    const ThemeColors *chrome[NP_FACE_COUNT];
} NowPlayingFaces;

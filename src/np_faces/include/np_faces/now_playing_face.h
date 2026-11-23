#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/canvas.h"
#include "gfx/image.h"
#include "gfx/rect.h"
#include "playback/player_status.h"
#include "settings/settings.h"
#include "ui/list_view.h"
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

typedef struct SceneCode {
    const uint8_t *code;
    uint32_t code_size;
    const char *strings;
    uint32_t strings_size;
    void *work;
    uint32_t work_size;
} SceneCode;

typedef struct SceneMenu {
    const ListModel *model;
    int selected;
    void (*draw)(void *context, Canvas *canvas, Rect area);
    int (*slot)(void *context, int position);
    void *context;
} SceneMenu;

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
    SceneCode scene;
    const SceneMenu *menu;
    uint8_t face;
    bool playing;
    bool scrubbing;
    bool volume_shown;
} NowPlayingFrame;

typedef struct NowPlayingFaces {
    uint32_t magic;
    void (*draw)(const NowPlayingFrame *frame, Canvas *canvas);
} NowPlayingFaces;

#pragma once

#include "np_faces/now_playing_face.h"

enum {
    SCENE_REGISTERS = 128,
    SCENE_ITEM_BASE = 32,
    SCENE_STRING_BASE = 64,
    SCENE_MOST_OPERANDS = 8,
    SCENE_MOST_STEPS = 20000,
    SCENE_WORK_BYTES = 1024
};

void scene_draw(const NowPlayingFrame *frame, Canvas *canvas);

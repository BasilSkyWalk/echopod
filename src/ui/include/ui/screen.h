#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/key_tracker.h"
#include "gfx/canvas.h"

typedef struct Screen Screen;

typedef enum ScreenActionType {
    SCREEN_ACTION_NONE,
    SCREEN_ACTION_PUSH,
    SCREEN_ACTION_POP,
    SCREEN_ACTION_POP_TO_ROOT
} ScreenActionType;

typedef struct ScreenAction {
    ScreenActionType type;
    Screen *target;
} ScreenAction;

struct Screen {
    const char *title;
    void *context;
    bool full_screen;
    bool hold_volume_keys;
    bool stays_when_idle;
    void (*enter)(void *context, uint32_t now_ms);
    ScreenAction (*handle_key)(void *context, KeyEvent event, uint32_t now_ms);
    void (*update)(void *context, uint32_t now_ms);
    void (*draw)(void *context, Canvas *canvas);
};

ScreenAction screen_action_none(void);
ScreenAction screen_action_push(Screen *target);
ScreenAction screen_action_pop(void);
ScreenAction screen_action_pop_to_root(void);

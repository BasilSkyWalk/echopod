#include "sim_input.h"

#include <stdbool.h>
#include <stddef.h>

#include "hal/input.h"

enum {
    TRANSITION_CAPACITY = 32
};

typedef struct KeyBinding {
    SDL_Keycode keycode;
    Key key;
} KeyBinding;

typedef struct TransitionQueue {
    HalKeyTransition items[TRANSITION_CAPACITY];
    int start;
    int length;
} TransitionQueue;

static const KeyBinding KEY_BINDINGS[] = {
    {SDLK_UP, KEY_VOLUME_UP},   {SDLK_DOWN, KEY_VOLUME_DOWN}, {SDLK_RETURN, KEY_PLAY},
    {SDLK_SPACE, KEY_PLAY},     {SDLK_m, KEY_MENU},           {SDLK_TAB, KEY_MENU},
    {SDLK_BACKSPACE, KEY_BACK}, {SDLK_ESCAPE, KEY_BACK},      {SDLK_p, KEY_POWER},
};

static TransitionQueue queue;

static bool find_binding(SDL_Keycode keycode, Key *key)
{
    for (size_t index = 0; index < sizeof KEY_BINDINGS / sizeof KEY_BINDINGS[0]; index++) {
        if (KEY_BINDINGS[index].keycode == keycode) {
            *key = KEY_BINDINGS[index].key;
            return true;
        }
    }
    return false;
}

static void enqueue(HalKeyTransition transition)
{
    if (queue.length == TRANSITION_CAPACITY) {
        return;
    }
    queue.items[(queue.start + queue.length) % TRANSITION_CAPACITY] = transition;
    queue.length++;
}

void sim_input_handle_event(const SDL_Event *event)
{
    bool is_key_event = event->type == SDL_KEYDOWN || event->type == SDL_KEYUP;
    if (!is_key_event || event->key.repeat != 0) {
        return;
    }
    Key key;
    if (!find_binding(event->key.keysym.sym, &key)) {
        return;
    }
    enqueue((HalKeyTransition){
        .key = key,
        .pressed = event->type == SDL_KEYDOWN,
        .time_ms = event->key.timestamp,
    });
}

void sim_input_inject(Key key, bool pressed, uint32_t time_ms)
{
    enqueue((HalKeyTransition){.key = key, .pressed = pressed, .time_ms = time_ms});
}

bool hal_input_next_transition(HalKeyTransition *transition)
{
    if (queue.length == 0) {
        return false;
    }
    *transition = queue.items[queue.start];
    queue.start = (queue.start + 1) % TRANSITION_CAPACITY;
    queue.length--;
    return true;
}

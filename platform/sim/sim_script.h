#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/key.h"

enum {
    SIM_SCRIPT_MAX_STEPS = 64,
    SIM_SCRIPT_MAX_EVENTS = SIM_SCRIPT_MAX_STEPS * 2,
    SIM_SCRIPT_TAP_FRAMES = 2,
    SIM_SCRIPT_HOLD_FRAMES = 70,
    SIM_SCRIPT_HELD_FRAMES = 300,
    SIM_SCRIPT_GAP_FRAMES = 8
};

typedef struct SimScriptStep {
    Key key;
    bool long_press;
    bool held;
} SimScriptStep;

typedef struct SimScript {
    SimScriptStep steps[SIM_SCRIPT_MAX_STEPS];
    int count;
} SimScript;

typedef struct SimScriptEvent {
    uint32_t frame;
    Key key;
    bool pressed;
} SimScriptEvent;

bool sim_script_parse(const char *text, SimScript *script);
int sim_script_schedule(const SimScript *script, uint32_t first_frame, SimScriptEvent *events);

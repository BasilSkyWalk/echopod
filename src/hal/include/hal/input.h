#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/key.h"

typedef struct HalKeyTransition {
    Key key;
    bool pressed;
    uint32_t time_ms;
} HalKeyTransition;

bool hal_input_next_transition(HalKeyTransition *transition);

#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    FADE_OPAQUE = 15
};

typedef struct FadeTiming {
    uint16_t fade_ms;
    uint16_t hold_ms;
    bool rising;
} FadeTiming;

typedef struct Fade {
    bool active;
    uint32_t start_ms;
    FadeTiming timing;
} Fade;

void fade_start(Fade *fade, uint32_t now_ms, FadeTiming timing);
uint8_t fade_alpha(const Fade *fade, uint32_t now_ms);
bool fade_finished(const Fade *fade, uint32_t now_ms);

#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    SLIDE_DURATION_MS = 240
};

typedef enum SlideDirection {
    SLIDE_FORWARD,
    SLIDE_BACK
} SlideDirection;

typedef struct Slide {
    bool active;
    uint32_t start_ms;
    SlideDirection direction;
} Slide;

void slide_start(Slide *slide, uint32_t now_ms, SlideDirection direction);
int slide_progress(const Slide *slide, uint32_t now_ms, int width);
bool slide_finished(const Slide *slide, uint32_t now_ms);
int slide_leaving_x(const Slide *slide, uint32_t now_ms, int width);
int slide_arriving_x(const Slide *slide, uint32_t now_ms, int width);
bool slide_shows_arriving_title(const Slide *slide, uint32_t now_ms);

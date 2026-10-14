#pragma once

#include <stdint.h>

#include "ui/screen.h"
#include "ui/theme.h"

enum {
    STOPWATCH_TEXT_CAPACITY = 16
};

typedef struct StopwatchScreen {
    Screen screen;
    const Theme *theme;
    uint32_t started_ms;
    char text[STOPWATCH_TEXT_CAPACITY];
    char clock[STOPWATCH_TEXT_CAPACITY];
} StopwatchScreen;

Screen *stopwatch_screen_init(StopwatchScreen *stopwatch, const Theme *theme);
const char *stopwatch_screen_text(const StopwatchScreen *stopwatch);

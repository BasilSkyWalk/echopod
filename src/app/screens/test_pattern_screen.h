#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/key.h"
#include "core/key_tracker.h"
#include "gfx/color.h"
#include "gfx/rect.h"
#include "ui/screen.h"

enum {
    TEST_PATTERN_FLASH_MS = 250
};

typedef struct KeyFlash {
    KeyGesture gesture;
    uint32_t until_ms;
    bool active;
} KeyFlash;

typedef struct TestPatternScreen {
    Screen screen;
    uint32_t started_at_ms;
    uint32_t now_ms;
    KeyFlash flashes[KEY_COUNT];
} TestPatternScreen;

Screen *test_pattern_screen_init(TestPatternScreen *pattern);
Rect test_pattern_key_indicator(Key key);
Color test_pattern_idle_color(void);
Color test_pattern_gesture_color(KeyGesture gesture);
Color test_pattern_first_bar_color(void);

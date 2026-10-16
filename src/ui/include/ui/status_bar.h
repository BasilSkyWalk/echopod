#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gfx/canvas.h"
#include "ui/theme.h"

typedef enum StatusPlayback {
    STATUS_PLAYBACK_NONE,
    STATUS_PLAYBACK_PLAYING,
    STATUS_PLAYBACK_PAUSED
} StatusPlayback;

enum {
    STATUS_BAR_LOW_BATTERY_PERCENT = 15,
    STATUS_BAR_PERCENT_CAPACITY = 8
};

typedef struct StatusBarState {
    StatusPlayback playback;
    bool battery_known;
    uint8_t battery_percent;
    bool charging;
} StatusBarState;

Rect status_bar_playback_area(const Theme *theme);
Rect status_bar_battery_area(const Theme *theme);
int status_bar_battery_fill_width(uint8_t percent, int inner_width);
void status_bar_percent_text(uint8_t percent, char *text, size_t capacity);
Rect status_bar_percent_area(const Theme *theme, const char *text);
void status_bar_draw(Canvas *canvas, const Theme *theme, StatusBarState state);

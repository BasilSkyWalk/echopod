#pragma once

#include <stdint.h>
#include <stddef.h>

#include "settings_store.h"
#include "ui/screen.h"
#include "ui/theme.h"
#include "ui/toast.h"

typedef struct ClockScreen {
    Screen screen;
    const Theme *theme;
    SettingsStore *store;
    Toast *toast;
    uint32_t started_ms;
    uint32_t now_ms;
    bool minute_field;
    char text[8];
} ClockScreen;

Screen *clock_screen_init(ClockScreen *clock, SettingsStore *store, Toast *toast,
                          const Theme *theme, uint32_t now_ms);
void clock_screen_format(const ClockScreen *clock, uint32_t now_ms, char *text, size_t capacity);
void clock_screen_commit(ClockScreen *clock, uint32_t now_ms);

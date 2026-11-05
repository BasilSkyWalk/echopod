#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "clock_store.h"
#include "settings_store.h"
#include "ui/screen.h"
#include "ui/theme.h"
#include "ui/toast.h"

enum {
    DATE_TIME_FIELD_COUNT = 6
};

typedef struct DateTimeConfig {
    ClockStore *clock;
    SettingsStore *store;
    Toast *toast;
    const Theme *theme;
} DateTimeConfig;

typedef struct DateTimeScreen {
    Screen screen;
    DateTimeConfig config;
    WallCivil civil;
    uint8_t zone;
    int field;
    bool editing;
    bool changed;
} DateTimeScreen;

Screen *date_time_screen_init(DateTimeScreen *screen, DateTimeConfig config);
ScreenAction date_time_screen_open(void *context);
const char *date_time_screen_summary(const void *context);

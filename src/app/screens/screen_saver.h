#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "clock_screen.h"
#include "cover_cache.h"
#include "player.h"
#include "settings_store.h"
#include "ui/screen.h"
#include "ui/theme.h"

typedef struct ScreenSaverScreen {
    Screen screen;
    const Theme *theme;
    SettingsStore *store;
    Player *player;
    CoverCache *covers;
    ClockScreen *clock;
    uint32_t now_ms;
    uint16_t album;
    bool has_album;
} ScreenSaverScreen;

Screen *screen_saver_init(ScreenSaverScreen *saver, SettingsStore *store, Player *player,
                          CoverCache *covers, ClockScreen *clock, const Theme *theme);

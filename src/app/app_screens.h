#pragma once

#include <stdbool.h>

#include "screens/font_sample_screen.h"
#include "screens/test_pattern_screen.h"
#include "ui/menu_screen.h"
#include "ui/message_screen.h"

enum {
    HOME_ITEM_COUNT = 4,
    MUSIC_ITEM_COUNT = 9,
    DIAGNOSTICS_ITEM_COUNT = 4
};

typedef struct AppOptions {
    bool show_frame_time;
} AppOptions;

typedef struct AppScreens {
    MenuScreen home;
    MenuScreen music;
    MenuScreen diagnostics;
    MenuScreen long_names;
    MessageScreen coming_soon;
    TestPatternScreen test_pattern;
    FontSampleScreen font_sample;
    MenuItem home_items[HOME_ITEM_COUNT];
    MenuItem music_items[MUSIC_ITEM_COUNT];
    MenuItem diagnostics_items[DIAGNOSTICS_ITEM_COUNT];
} AppScreens;

Screen *app_screens_init(AppScreens *screens, AppOptions *options, const Theme *theme);

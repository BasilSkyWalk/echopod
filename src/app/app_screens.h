#pragma once

#include <stdbool.h>

#include "browse_screen.h"
#include "library_store.h"
#include "player.h"
#include "screens/font_sample_screen.h"
#include "screens/now_playing_screen.h"
#include "screens/test_pattern_screen.h"
#include "ui/menu_screen.h"
#include "ui/message_screen.h"

enum {
    HOME_ITEM_COUNT = 4,
    MUSIC_ITEM_COUNT = 9,
    SETTINGS_ITEM_COUNT = 2,
    DIAGNOSTICS_ITEM_COUNT = 4
};

typedef struct AppOptions {
    bool show_frame_time;
} AppOptions;

typedef struct BrowseEntry {
    BrowsePool *pool;
    const LibraryStore *store;
    Screen *unavailable;
    BrowseQuery query;
} BrowseEntry;

typedef struct AppScreens {
    MenuScreen home;
    MenuScreen music;
    MenuScreen settings;
    MenuScreen diagnostics;
    MenuScreen long_names;
    MessageScreen coming_soon;
    MessageScreen no_library;
    BrowsePool browse;
    NowPlayingScreen now_playing;
    BrowseEntry browse_entries[MUSIC_ITEM_COUNT];
    TestPatternScreen test_pattern;
    FontSampleScreen font_sample;
    MenuItem home_items[HOME_ITEM_COUNT];
    MenuItem music_items[MUSIC_ITEM_COUNT];
    MenuItem settings_items[SETTINGS_ITEM_COUNT];
    MenuItem diagnostics_items[DIAGNOSTICS_ITEM_COUNT];
} AppScreens;

typedef struct AppScreensContext {
    AppOptions *options;
    const Theme *theme;
    const LibraryStore *store;
    Player *player;
} AppScreensContext;

Screen *app_screens_init(AppScreens *screens, AppScreensContext context);

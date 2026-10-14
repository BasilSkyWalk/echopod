#pragma once

#include <stdbool.h>

#include "browse_screen.h"
#include "core/power_policy.h"
#include "cover_cache.h"
#include "favorites_store.h"
#include "library_store.h"
#include "player.h"
#include "screens/cover_flow_screen.h"
#include "screens/font_sample_screen.h"
#include "screens/now_playing_screen.h"
#include "screens/stopwatch_screen.h"
#include "screens/test_pattern_screen.h"
#include "settings/settings.h"
#include "settings_screens.h"
#include "settings_store.h"
#include "ui/menu_screen.h"
#include "ui/message_screen.h"
#include "ui/toast.h"

enum {
    HOME_ITEM_COUNT = 4,
    MUSIC_ITEM_COUNT = 9,
    DIAGNOSTICS_ITEM_COUNT = 7,
    START_PATH_CAPACITY = 2
};

typedef struct AppOptions {
    bool show_frame_time;
} AppOptions;

typedef struct ClockOption {
    bool failed;
} ClockOption;

typedef struct BrowseEntry {
    BrowsePool *pool;
    const LibraryStore *store;
    Screen *unavailable;
    BrowseQuery query;
    bool favorites;
    Screen *fixed;
} BrowseEntry;

typedef struct AppScreens {
    MenuScreen home;
    MenuScreen music;
    SettingsScreens settings;
    MenuScreen diagnostics;
    MenuScreen long_names;
    MessageScreen no_library;
    BrowsePool browse;
    NowPlayingScreen now_playing;
    CoverFlowScreen cover_flow;
    BrowseEntry browse_entries[MUSIC_ITEM_COUNT];
    TestPatternScreen test_pattern;
    FontSampleScreen font_sample;
    StopwatchScreen stopwatch;
    ClockOption clock;
    MenuItem home_items[HOME_ITEM_COUNT];
    MenuItem music_items[MUSIC_ITEM_COUNT];
    MenuItem diagnostics_items[DIAGNOSTICS_ITEM_COUNT];
} AppScreens;

typedef struct AppScreensContext {
    AppOptions *options;
    const Theme *theme;
    const LibraryStore *store;
    Player *player;
    SettingsStore *settings;
    FavoritesStore *favorites;
    PowerPolicy *power;
    Toast *toast;
    CoverCache *covers;
} AppScreensContext;

Screen *app_screens_init(AppScreens *screens, AppScreensContext context);
int app_screens_start_path(AppScreens *screens, StartScreen start, Screen **path);

#pragma once

#include <stdbool.h>

#include "browse_screen.h"
#include "core/power_policy.h"
#include "cover_cache.h"
#include "diagnostics_values.h"
#include "favorites_store.h"
#include "hal/audio.h"
#include "home_art.h"
#include "library_store.h"
#include "player.h"
#include "screens/bluetooth_screen.h"
#include "screens/brick_screen.h"
#include "screens/clock_screen.h"
#include "screens/cover_flow_screen.h"
#include "screens/equalizer_screen.h"
#include "screens/now_playing_screen.h"
#include "screens/music_quiz_screen.h"
#include "screens/screen_saver.h"
#include "screens/test_pattern_screen.h"
#include "screens/up_next_screen.h"
#include "settings/settings.h"
#include "settings_screens.h"
#include "settings_store.h"
#include "ui/menu_screen.h"
#include "ui/message_screen.h"
#include "ui/toast.h"

enum {
    DIAGNOSTICS_ITEM_COUNT = 16,
    HOME_ROOT_ITEM_COUNT = 6,
    HOME_MUSIC_SHORTCUT_COUNT = 9,
    HOME_SETTINGS_SHORTCUT_COUNT = 20,
    HOME_DIAGNOSTICS_SHORTCUT_COUNT = DIAGNOSTICS_ITEM_COUNT,
    HOME_ITEM_COUNT = HOME_ROOT_ITEM_COUNT + HOME_MUSIC_SHORTCUT_COUNT +
                      HOME_SETTINGS_SHORTCUT_COUNT + HOME_DIAGNOSTICS_SHORTCUT_COUNT + 2,
    SHELF_COUNT = 2,
    MUSIC_ITEM_COUNT = 9,
    START_PATH_CAPACITY = 2
};

typedef struct AppOptions {
    bool show_frame_time;
    bool ignore_power_taps;
} AppOptions;

typedef struct ClockOption {
    bool failed;
} ClockOption;

typedef struct Core1Option {
    bool run;
    HalCore1Test test;
} Core1Option;

typedef struct Core1AccelOption {
    bool run;
    HalCore1Accel result;
} Core1AccelOption;

typedef struct BrowseEntry {
    BrowsePool *pool;
    const LibraryStore *store;
    Screen *unavailable;
    BrowseQuery query;
    bool favorites;
    Screen *fixed;
    SpokenKind shelf;
    bool present;
} BrowseEntry;

typedef struct HomeMenuOption {
    SettingsStore *store;
    Toast *toast;
    uint8_t bit;
    bool available;
    bool (*was_shown)(const void *context);
    const void *shown_context;
} HomeMenuOption;

typedef struct AppScreens {
    MenuScreen home;
    MenuScreen main_menu_customization;
    MenuScreen music;
    SettingsScreens settings;
    MenuScreen diagnostics;
    MessageScreen no_library;
    BrowsePool browse;
    NowPlayingScreen now_playing;
    UpNextScreen up_next;
    CoverFlowScreen cover_flow;
    BrowseEntry browse_entries[MUSIC_ITEM_COUNT];
    BrowseEntry shelf_entries[SHELF_COUNT];
    TestPatternScreen test_pattern;
    BrickScreen brick;
    MusicQuizScreen music_quiz;
    ClockScreen clock_screen;
    ScreenSaverScreen screen_saver;
    BluetoothScreen bluetooth;
    EqualizerScreen equalizer;
    ClockOption clock;
    Core1Option core1;
    Core1AccelOption core1_accel;
    MenuItem home_items[HOME_ITEM_COUNT];
    MenuItem customization_items[HOME_ITEM_COUNT];
    HomeMenuOption home_options[HOME_ITEM_COUNT];
    int customization_item_count;
    HomeArt home_art;
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
    bool *rescan_requested;
    SpokenStore *spoken;
    BluetoothLink *bluetooth;
    EqualizerStore *equalizer;
} AppScreensContext;

Screen *app_screens_init(AppScreens *screens, AppScreensContext context);
MenuItem app_screens_open_item(const char *label, Screen *target);
Screen *app_screens_build_menu(MenuScreen *menu, const char *title, const MenuItem *items,
                               int count, const Theme *theme);
void diagnostics_menu_build(AppScreens *screens, AppOptions *options, const Theme *theme);
int app_screens_start_path(AppScreens *screens, StartScreen start, Screen **path);

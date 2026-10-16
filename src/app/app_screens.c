#include "app_screens.h"

#include <stdbool.h>
#include <stddef.h>

#include "diagnostics_values.h"
#include "hal/system.h"

typedef enum MusicSource {
    MUSIC_COVER_FLOW,
    MUSIC_LIBRARY,
    MUSIC_FAVORITES
} MusicSource;

typedef struct MusicEntry {
    const char *label;
    MusicSource source;
    BrowseKind kind;
} MusicEntry;

enum {
    MUSIC_COVER_FLOW_ENTRY = 0,
    MUSIC_ARTISTS_ENTRY = 1,
    MUSIC_ALBUMS_ENTRY = 3,
    MUSIC_PLAYLISTS_ENTRY = 7,
    MUSIC_FAVORITES_ENTRY = 8
};

static const MusicEntry MUSIC_ENTRIES[MUSIC_ITEM_COUNT] = {
    {"Cover Flow", MUSIC_COVER_FLOW, BROWSE_ALBUMS},
    {"Artists", MUSIC_LIBRARY, BROWSE_ARTISTS},
    {"Album Artists", MUSIC_LIBRARY, BROWSE_ALBUM_ARTISTS},
    {"Albums", MUSIC_LIBRARY, BROWSE_ALBUMS},
    {"Genres", MUSIC_LIBRARY, BROWSE_GENRES},
    {"Folders", MUSIC_LIBRARY, BROWSE_FOLDER},
    {"All Songs", MUSIC_LIBRARY, BROWSE_ALL_SONGS},
    {"Playlists", MUSIC_LIBRARY, BROWSE_PLAYLISTS},
    {"Favorites", MUSIC_FAVORITES, BROWSE_ALBUMS},
};

static const char *year_value(const void *context)
{
    return context;
}

static const MenuItem LONG_NAME_ITEMS[] = {
    {"Sigur Rós — Ágætis byrjun", NULL, "1999", year_value},
    {"Godspeed You! Black Emperor — Lift Your Skinny Fists Like Antennas to Heaven", NULL, "2000",
     year_value},
    {"Björk — Homogenic", NULL, "1997", year_value},
    {"Sufjan Stevens — Illinois (Come On Feel the Illinoise)", NULL, "2005", year_value},
    {"Motörhead — Ace of Spades", NULL, "1980", year_value},
    {"Jóhann Jóhannsson — Orphée", NULL, "2016", year_value},
    {"Ólafur Arnalds — re:member", NULL, "2018", year_value},
    {"Röyksopp — Melody A.M.", NULL, "2001", year_value},
    {"Café Tacvba — Re", NULL, "1994", year_value},
    {"Beyoncé — Lemonade", NULL, "2016", year_value},
    {"Mötley Crüe — Dr. Feelgood", NULL, "1989", year_value},
    {"Daft Punk — Random Access Memories", NULL, "2013", year_value},
    {"Explosions in the Sky — The Earth Is Not a Cold Dead Place", NULL, "2003", year_value},
    {"Hania Rani — Esja", NULL, "2019", year_value},
    {"Nils Frahm — All Melody", NULL, "2018", year_value},
    {"The Avalanches — Since I Left You", NULL, "2000", year_value},
};

enum {
    LONG_NAME_COUNT = sizeof LONG_NAME_ITEMS / sizeof LONG_NAME_ITEMS[0]
};

static ScreenAction toggle_frame_time(void *context)
{
    AppOptions *options = context;
    options->show_frame_time = !options->show_frame_time;
    return screen_action_none();
}

static const char *frame_time_value(const void *context)
{
    const AppOptions *options = context;
    return options->show_frame_time ? "On" : "Off";
}

static const char *firmware_value(const void *context)
{
    (void)context;
    return hal_system_version();
}

static char clock_text[DIAGNOSTICS_VALUE_CAPACITY];

static ScreenAction cycle_clock(void *context)
{
    ClockOption *clock = context;
    clock->failed = !hal_system_set_clock_mhz(diagnostics_next_clock_mhz(hal_system_clock_mhz()));
    return screen_action_none();
}

static const char *clock_value(const void *context)
{
    const ClockOption *clock = context;
    diagnostics_clock_text(hal_system_clock_mhz(), clock->failed, clock_text, sizeof clock_text);
    return clock_text;
}

static char loop_text[DIAGNOSTICS_VALUE_CAPACITY];

static ScreenAction run_loop_benchmark(void *context)
{
    LoopOption *loop = context;
    loop->cycles = hal_system_loop_cycles();
    loop->run = true;
    return screen_action_none();
}

static const char *loop_value(const void *context)
{
    const LoopOption *loop = context;
    diagnostics_loop_text(loop->run, loop->cycles, loop_text, sizeof loop_text);
    return loop_text;
}

static ScreenAction play_test_tone(void *context)
{
    ToneOption *tone = context;
    tone->result = hal_audio_test_tone();
    tone->tried = true;
    return screen_action_none();
}

static char tone_text[DIAGNOSTICS_VALUE_CAPACITY];
static char boot_clocks_text[DIAGNOSTICS_VALUE_CAPACITY];
static char boot_pll_text[DIAGNOSTICS_VALUE_CAPACITY];

static const char *tone_value(const void *context)
{
    const ToneOption *tone = context;
    if (!tone->tried) {
        return "";
    }
    diagnostics_tone_text(tone->result, hal_audio_tone_detail(), tone_text, sizeof tone_text);
    return tone_text;
}

static const char *boot_clocks_value(const void *context)
{
    (void)context;
    diagnostics_boot_clocks_text(hal_system_boot_clocks(), boot_clocks_text,
                                 sizeof boot_clocks_text);
    return boot_clocks_text;
}

static const char *boot_pll_value(const void *context)
{
    (void)context;
    diagnostics_boot_pll_text(hal_system_boot_clocks(), boot_pll_text, sizeof boot_pll_text);
    return boot_pll_text;
}

static ScreenAction toggle_dma_tone(void *context)
{
    DmaToneOption *tone = context;
    if (hal_audio_dma_tone_status().running) {
        hal_audio_dma_tone_stop();
        tone->stopped = true;
        return screen_action_none();
    }
    tone->result = hal_audio_dma_tone_start();
    tone->tried = true;
    tone->stopped = false;
    return screen_action_none();
}

static char dma_tone_text[DIAGNOSTICS_VALUE_CAPACITY];

static const char *dma_tone_value(const void *context)
{
    const DmaToneOption *tone = context;
    DmaToneView view = {.tried = tone->tried,
                        .stopped = tone->stopped,
                        .result = tone->result,
                        .status = hal_audio_dma_tone_status(),
                        .detail = hal_audio_tone_detail()};
    diagnostics_dma_tone_text(view, dma_tone_text, sizeof dma_tone_text);
    return dma_tone_text;
}

static MenuItem open_item(const char *label, Screen *target)
{
    return (MenuItem){.label = label, .select = menu_open_screen, .context = target, .value = NULL};
}

static Screen *build_menu(MenuScreen *menu, const char *title, const MenuItem *items, int count,
                          const Theme *theme)
{
    return menu_screen_init(menu, (MenuConfig){title, items, count, theme});
}

static Screen *browse_screen_for(BrowseEntry *entry)
{
    if (!entry->store->available) {
        return entry->unavailable;
    }
    if (entry->fixed != NULL) {
        return entry->fixed;
    }
    if (entry->favorites) {
        return browse_pool_open_favorites(entry->pool, 0);
    }
    return browse_pool_open(entry->pool, 0, entry->query);
}

static ScreenAction open_browse(void *context)
{
    return screen_action_push(browse_screen_for(context));
}

static MenuItem music_item(AppScreens *screens, const LibraryStore *store, int index)
{
    const MusicEntry *entry = &MUSIC_ENTRIES[index];
    screens->browse_entries[index] = (BrowseEntry){
        .pool = &screens->browse,
        .store = store,
        .unavailable = &screens->no_library.screen,
        .query = browse_root_query(entry->kind),
        .favorites = entry->source == MUSIC_FAVORITES,
        .fixed = entry->source == MUSIC_COVER_FLOW ? &screens->cover_flow.screen : NULL,
    };
    return (MenuItem){.label = entry->label,
                      .select = open_browse,
                      .context = &screens->browse_entries[index],
                      .value = NULL};
}

static void build_music(AppScreens *screens, AppScreensContext context)
{
    BrowsePoolConfig browse = {
        .library = &context.store->library,
        .theme = context.theme,
        .player = context.player,
        .now_playing = &screens->now_playing.screen,
        .favorites = context.favorites,
        .toast = context.toast,
    };
    browse_pool_init(&screens->browse, browse);
    CoverFlowConfig cover_flow = {
        .library = &context.store->library,
        .theme = context.theme,
        .covers = context.covers,
        .favorites = context.favorites,
        .toast = context.toast,
        .browse = &screens->browse,
        .player = context.player,
    };
    cover_flow_screen_init(&screens->cover_flow, cover_flow);
    for (int index = 0; index < MUSIC_ITEM_COUNT; index++) {
        screens->music_items[index] = music_item(screens, context.store, index);
    }
    build_menu(&screens->music, "Music", screens->music_items, MUSIC_ITEM_COUNT, context.theme);
}

static void build_settings(AppScreens *screens, AppScreensContext context)
{
    SettingsContext settings = {
        .store = context.settings,
        .player = context.player,
        .power = context.power,
        .toast = context.toast,
        .sleep_option = 0,
    };
    settings_screens_init(&screens->settings, settings, context.theme);
}

static void build_diagnostics(AppScreens *screens, AppOptions *options, const Theme *theme)
{
    Screen *long_names =
        build_menu(&screens->long_names, "Long names", LONG_NAME_ITEMS, LONG_NAME_COUNT, theme);
    screens->diagnostics_items[0] =
        open_item("Key test", test_pattern_screen_init(&screens->test_pattern));
    screens->diagnostics_items[1] =
        open_item("Fonts", font_sample_screen_init(&screens->font_sample, theme));
    screens->diagnostics_items[2] = open_item("Long names", long_names);
    screens->diagnostics_items[3] = (MenuItem){
        .label = "Frame time",
        .select = toggle_frame_time,
        .context = options,
        .value = frame_time_value,
    };
    screens->diagnostics_items[4] =
        (MenuItem){.label = "Firmware", .select = NULL, .context = NULL, .value = firmware_value};
    screens->clock = (ClockOption){.failed = false};
    screens->diagnostics_items[5] = (MenuItem){.label = "CPU clock",
                                               .select = cycle_clock,
                                               .context = &screens->clock,
                                               .value = clock_value};
    screens->diagnostics_items[6] =
        open_item("Stopwatch", stopwatch_screen_init(&screens->stopwatch, theme));
    screens->tone = (ToneOption){.tried = false, .result = HAL_TONE_PLAYED};
    screens->diagnostics_items[7] = (MenuItem){.label = "Test tone",
                                               .select = play_test_tone,
                                               .context = &screens->tone,
                                               .value = tone_value};
    screens->dma_tone =
        (DmaToneOption){.tried = false, .stopped = false, .result = HAL_TONE_PLAYED};
    screens->diagnostics_items[8] = (MenuItem){.label = "DMA tone",
                                               .select = toggle_dma_tone,
                                               .context = &screens->dma_tone,
                                               .value = dma_tone_value};
    screens->diagnostics_items[9] = (MenuItem){
        .label = "Boot clocks", .select = NULL, .context = NULL, .value = boot_clocks_value};
    screens->diagnostics_items[10] =
        (MenuItem){.label = "Boot PLL", .select = NULL, .context = NULL, .value = boot_pll_value};
    screens->loop = (LoopOption){.run = false};
    screens->diagnostics_items[11] = (MenuItem){.label = "Loop cycles",
                                                .select = run_loop_benchmark,
                                                .context = &screens->loop,
                                                .value = loop_value};
    build_menu(&screens->diagnostics, "Diagnostics", screens->diagnostics_items,
               DIAGNOSTICS_ITEM_COUNT, theme);
}

static void build_home(AppScreens *screens, const Theme *theme)
{
    screens->home_items[0] = open_item("Music", &screens->music.screen);
    screens->home_items[1] = open_item("Now Playing", &screens->now_playing.screen);
    screens->home_items[2] = open_item("Settings", &screens->settings.menu.screen);
    screens->home_items[3] = open_item("Diagnostics", &screens->diagnostics.screen);
    build_menu(&screens->home, "Echo Mini", screens->home_items, HOME_ITEM_COUNT, theme);
}

Screen *app_screens_init(AppScreens *screens, AppScreensContext context)
{
    const Theme *theme = context.theme;
    MessageConfig no_library = {
        .title = "No library", .message = "Run the PC indexer to build one", .theme = theme};
    NowPlayingConfig now_playing = {.player = context.player,
                                    .library = &context.store->library,
                                    .theme = theme,
                                    .covers = context.covers};
    message_screen_init(&screens->no_library, no_library);
    now_playing_screen_init(&screens->now_playing, now_playing);
    build_music(screens, context);
    build_settings(screens, context);
    build_diagnostics(screens, context.options, theme);
    build_home(screens, theme);
    return &screens->home.screen;
}

static int music_path(AppScreens *screens, int entry, Screen **path)
{
    BrowseEntry *browse = &screens->browse_entries[entry];
    if (!browse->store->available) {
        return 0;
    }
    list_view_select(&screens->music.list, entry, 0);
    path[0] = &screens->music.screen;
    path[1] = browse_screen_for(browse);
    return path[1] != NULL ? START_PATH_CAPACITY : 1;
}

int app_screens_start_path(AppScreens *screens, StartScreen start, Screen **path)
{
    switch (start) {
    case START_MAIN_MENU:
        return 0;
    case START_NOW_PLAYING:
        list_view_select(&screens->home.list, 1, 0);
        path[0] = &screens->now_playing.screen;
        return 1;
    case START_ALBUMS:
        return music_path(screens, MUSIC_ALBUMS_ENTRY, path);
    case START_ARTISTS:
        return music_path(screens, MUSIC_ARTISTS_ENTRY, path);
    case START_FAVORITES:
        return music_path(screens, MUSIC_FAVORITES_ENTRY, path);
    case START_PLAYLISTS:
        return music_path(screens, MUSIC_PLAYLISTS_ENTRY, path);
    case START_COVER_FLOW:
        return music_path(screens, MUSIC_COVER_FLOW_ENTRY, path);
    }
    return 0;
}

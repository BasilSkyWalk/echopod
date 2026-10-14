#include "app_screens.h"

#include "hal/clock.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct MusicEntry {
    const char *label;
    bool from_library;
    BrowseKind kind;
} MusicEntry;

static const MusicEntry MUSIC_ENTRIES[MUSIC_ITEM_COUNT] = {
    {"Cover Flow", false, BROWSE_ALBUMS},
    {"Artists", true, BROWSE_ARTISTS},
    {"Album Artists", true, BROWSE_ALBUM_ARTISTS},
    {"Albums", true, BROWSE_ALBUMS},
    {"Genres", true, BROWSE_GENRES},
    {"Folders", true, BROWSE_FOLDER},
    {"All Songs", true, BROWSE_ALL_SONGS},
    {"Playlists", false, BROWSE_ALBUMS},
    {"Favorites", false, BROWSE_ALBUMS},
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

static MenuItem open_item(const char *label, Screen *target)
{
    return (MenuItem){.label = label, .select = menu_open_screen, .context = target, .value = NULL};
}

static Screen *build_menu(MenuScreen *menu, const char *title, const MenuItem *items, int count,
                          const Theme *theme)
{
    return menu_screen_init(menu, (MenuConfig){title, items, count, theme});
}

static ScreenAction open_browse(void *context)
{
    BrowseEntry *entry = context;
    if (!entry->store->available) {
        return screen_action_push(entry->unavailable);
    }
    return screen_action_push(browse_pool_open(entry->pool, 0, entry->query));
}

static MenuItem music_item(AppScreens *screens, const LibraryStore *store, int index)
{
    const MusicEntry *entry = &MUSIC_ENTRIES[index];
    if (!entry->from_library) {
        return open_item(entry->label, &screens->coming_soon.screen);
    }
    screens->browse_entries[index] = (BrowseEntry){
        .pool = &screens->browse,
        .store = store,
        .unavailable = &screens->no_library.screen,
        .query = browse_root_query(entry->kind),
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
    };
    browse_pool_init(&screens->browse, browse);
    for (int index = 0; index < MUSIC_ITEM_COUNT; index++) {
        screens->music_items[index] = music_item(screens, context.store, index);
    }
    build_menu(&screens->music, "Music", screens->music_items, MUSIC_ITEM_COUNT, context.theme);
}

static const char *const SHUFFLE_VALUES[SHUFFLE_MODE_COUNT] = {"Off", "Songs", "Albums"};
static const char *const REPEAT_VALUES[REPEAT_MODE_COUNT] = {"Off", "One", "All"};

static ScreenAction cycle_shuffle(void *context)
{
    Player *player = context;
    ShuffleMode next = (ShuffleMode)((player_status(player).shuffle + 1) % SHUFFLE_MODE_COUNT);
    player_set_shuffle(player, next, hal_clock_microseconds());
    return screen_action_none();
}

static const char *shuffle_value(const void *context)
{
    return SHUFFLE_VALUES[player_status(context).shuffle];
}

static ScreenAction cycle_repeat(void *context)
{
    Player *player = context;
    player_set_repeat(player, (RepeatMode)((player_status(player).repeat + 1) % REPEAT_MODE_COUNT));
    return screen_action_none();
}

static const char *repeat_value(const void *context)
{
    return REPEAT_VALUES[player_status(context).repeat];
}

static void build_settings(AppScreens *screens, AppScreensContext context)
{
    screens->settings_items[0] = (MenuItem){.label = "Shuffle",
                                            .select = cycle_shuffle,
                                            .context = context.player,
                                            .value = shuffle_value};
    screens->settings_items[1] = (MenuItem){.label = "Repeat",
                                            .select = cycle_repeat,
                                            .context = context.player,
                                            .value = repeat_value};
    build_menu(&screens->settings, "Settings", screens->settings_items, SETTINGS_ITEM_COUNT,
               context.theme);
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
    build_menu(&screens->diagnostics, "Diagnostics", screens->diagnostics_items,
               DIAGNOSTICS_ITEM_COUNT, theme);
}

static void build_home(AppScreens *screens, const Theme *theme)
{
    screens->home_items[0] = open_item("Music", &screens->music.screen);
    screens->home_items[1] = open_item("Now Playing", &screens->now_playing.screen);
    screens->home_items[2] = open_item("Settings", &screens->settings.screen);
    screens->home_items[3] = open_item("Diagnostics", &screens->diagnostics.screen);
    build_menu(&screens->home, "Echo Mini", screens->home_items, HOME_ITEM_COUNT, theme);
}

Screen *app_screens_init(AppScreens *screens, AppScreensContext context)
{
    const Theme *theme = context.theme;
    MessageConfig coming_soon = {
        .title = "Coming soon", .message = "This part is not built yet", .theme = theme};
    MessageConfig no_library = {
        .title = "No library", .message = "Run the PC indexer to build one", .theme = theme};
    NowPlayingConfig now_playing = {
        .player = context.player, .library = &context.store->library, .theme = theme};
    message_screen_init(&screens->coming_soon, coming_soon);
    message_screen_init(&screens->no_library, no_library);
    now_playing_screen_init(&screens->now_playing, now_playing);
    build_music(screens, context);
    build_settings(screens, context);
    build_diagnostics(screens, context.options, theme);
    build_home(screens, theme);
    return &screens->home.screen;
}

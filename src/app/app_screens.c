#include "app_screens.h"

#include <stdbool.h>
#include <stddef.h>

#include "diagnostics_values.h"
#include "hal/power.h"
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

static bool player_has_track(const void *context)
{
    return player_status(context).has_track;
}

MenuItem app_screens_open_item(const char *label, Screen *target)
{
    return (MenuItem){.label = label,
                      .select = menu_open_screen,
                      .context = target,
                      .value = NULL,
                      .opens = true};
}

Screen *app_screens_build_menu(MenuScreen *menu, const char *title, const MenuItem *items,
                               int count, const Theme *theme)
{
    return menu_screen_init(
        menu, (MenuConfig){.title = title, .items = items, .item_count = count, .theme = theme});
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
    if (entry->shelf != SPOKEN_NONE) {
        return browse_pool_open_shelf(entry->pool, 0, entry->shelf);
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
        .shelf = SPOKEN_NONE,
        .present = true,
    };
    return (MenuItem){.label = entry->label,
                      .select = open_browse,
                      .context = &screens->browse_entries[index],
                      .value = NULL,
                      .opens = true};
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
        .spoken = context.spoken,
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
    app_screens_build_menu(&screens->music, "Music", screens->music_items, MUSIC_ITEM_COUNT,
                           context.theme);
}

static void build_settings(AppScreens *screens, AppScreensContext context)
{
    SettingsContext settings = {
        .store = context.settings,
        .player = context.player,
        .power = context.power,
        .toast = context.toast,
        .rescan_requested = context.rescan_requested,
        .bluetooth = &screens->bluetooth,
        .sleep_option = 0,
    };
    settings_screens_init(&screens->settings, settings, context.theme);
}

static bool shelf_present(const void *context)
{
    const BrowseEntry *entry = context;
    return entry->present;
}

static MenuItem shelf_item(AppScreens *screens, AppScreensContext context, SpokenKind shelf)
{
    uint16_t folder = 0;
    BrowseEntry *entry = &screens->shelf_entries[shelf == SPOKEN_PODCAST ? 1 : 0];
    *entry = (BrowseEntry){
        .pool = &screens->browse,
        .store = context.store,
        .unavailable = &screens->no_library.screen,
        .shelf = shelf,
        .present = context.store->available &&
                   spoken_shelf_folder(&context.store->library, shelf, &folder),
    };
    return (MenuItem){.label = shelf == SPOKEN_PODCAST ? "Podcasts" : "Audiobooks",
                      .select = open_browse,
                      .context = entry,
                      .opens = true,
                      .shown = shelf_present,
                      .shown_context = entry};
}

static void build_home(AppScreens *screens, AppScreensContext context)
{
    const Theme *theme = context.theme;
    screens->home_items[0] = app_screens_open_item("Music", &screens->music.screen);
    screens->home_items[1] = shelf_item(screens, context, SPOKEN_AUDIOBOOK);
    screens->home_items[2] = shelf_item(screens, context, SPOKEN_PODCAST);
    screens->home_items[3] = app_screens_open_item("Settings", &screens->settings.menu.screen);
    screens->home_items[4] = app_screens_open_item("Diagnostics", &screens->diagnostics.screen);
    screens->home_items[5] = app_screens_open_item("Now Playing", &screens->now_playing.screen);
    screens->home_items[5].shown = player_has_track;
    screens->home_items[5].shown_context = context.player;
    home_art_init(&screens->home_art, context.covers, context.store, context.player, theme);
    MenuConfig home = {.title = "EchoPod",
                       .items = screens->home_items,
                       .item_count = HOME_ITEM_COUNT,
                       .theme = theme,
                       .panel = {.width = HOME_ART_WIDTH,
                                 .update = home_art_update,
                                 .draw = home_art_draw,
                                 .context = &screens->home_art}};
    menu_screen_init(&screens->home, home);
}

Screen *app_screens_init(AppScreens *screens, AppScreensContext context)
{
    const Theme *theme = context.theme;
    MessageConfig no_library = {
        .title = "No library", .message = "Run the PC indexer to build one", .theme = theme};
    UpNextConfig up_next = {
        .library = &context.store->library, .theme = theme, .player = context.player};
    NowPlayingConfig now_playing = {.player = context.player,
                                    .library = &context.store->library,
                                    .theme = theme,
                                    .covers = context.covers,
                                    .up_next = up_next_screen_init(&screens->up_next, up_next)};
    message_screen_init(&screens->no_library, no_library);
    bluetooth_screen_init(&screens->bluetooth, context.bluetooth, theme);
    now_playing_screen_init(&screens->now_playing, now_playing);
    build_music(screens, context);
    build_settings(screens, context);
    diagnostics_menu_build(screens, context.options, theme);
    build_home(screens, context);
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

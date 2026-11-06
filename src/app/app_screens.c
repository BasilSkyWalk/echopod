#include "app_screens.h"

#include <stdbool.h>
#include <stddef.h>

#include "core/i18n.h"
#include "diagnostics_values.h"
#include "hal/power.h"
#include "hal/system.h"

typedef enum MusicSource {
    MUSIC_COVER_FLOW,
    MUSIC_LIBRARY,
    MUSIC_FAVORITES
} MusicSource;

typedef struct MusicEntry {
    StringId label;
    MusicSource source;
    BrowseKind kind;
} MusicEntry;

enum {
    START_HOME_ONLY = -1
};

static const int8_t START_ENTRIES[START_SCREEN_COUNT] = {
    [START_MAIN_MENU] = START_HOME_ONLY,
    [START_NOW_PLAYING] = START_HOME_ONLY,
    [START_ALBUMS] = 3,
    [START_ARTISTS] = 1,
    [START_FAVORITES] = 8,
    [START_PLAYLISTS] = 7,
    [START_COVER_FLOW] = 0,
    [START_ALBUM_ARTISTS] = 2,
    [START_GENRES] = 4,
    [START_FOLDERS] = 5,
    [START_ALL_SONGS] = 6,
};

static const MusicEntry MUSIC_ENTRIES[MUSIC_ITEM_COUNT] = {
    {STR_MENU_COVER_FLOW, MUSIC_COVER_FLOW, BROWSE_ALBUMS},
    {STR_MENU_ARTISTS, MUSIC_LIBRARY, BROWSE_ARTISTS},
    {STR_MENU_ALBUM_ARTISTS, MUSIC_LIBRARY, BROWSE_ALBUM_ARTISTS},
    {STR_MENU_ALBUMS, MUSIC_LIBRARY, BROWSE_ALBUMS},
    {STR_MENU_GENRES, MUSIC_LIBRARY, BROWSE_GENRES},
    {STR_MENU_FOLDERS, MUSIC_LIBRARY, BROWSE_FOLDER},
    {STR_MENU_ALL_SONGS, MUSIC_LIBRARY, BROWSE_ALL_SONGS},
    {STR_MENU_PLAYLISTS, MUSIC_LIBRARY, BROWSE_PLAYLISTS},
    {STR_MENU_FAVORITES, MUSIC_FAVORITES, BROWSE_ALBUMS},
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
    if (entry->flow != NULL && entry->query.kind != BROWSE_FOLDER) {
        return cover_flow_screen_open_all(entry->flow);
    }
    const uint8_t *options = entry->pool->config.options;
    if (entry->flow != NULL && options != NULL && options[OPTION_FOLDER_VIEW] != 0) {
        Screen *folders = cover_flow_screen_open_group(entry->flow, LIBRARY_FOLDERS, 0, 0);
        if (folders != NULL) {
            return folders;
        }
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
        .flow = entry->source == MUSIC_COVER_FLOW || entry->kind == BROWSE_FOLDER
                    ? &screens->cover_flow
                    : NULL,
        .shelf = SPOKEN_NONE,
        .present = true,
    };
    return (MenuItem){.label = i18n_text(entry->label),
                      .select = open_browse,
                      .context = &screens->browse_entries[index],
                      .value = NULL,
                      .opens = true};
}

static Screen *open_group_flow(void *context, LibrarySection group, uint16_t subject, int level)
{
    return cover_flow_screen_open_group(context, group, subject, level);
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
        .options = context.settings->settings.options,
        .open_flow = open_group_flow,
        .flow_context = &screens->cover_flow,
    };
    browse_pool_init(&screens->browse, browse);
    CoverFlowConfig cover_flow = {
        .library = &context.store->library,
        .theme = context.stage_theme != NULL ? context.stage_theme : context.theme,
        .covers = context.covers,
        .favorites = context.favorites,
        .toast = context.toast,
        .browse = &screens->browse,
        .player = context.player,
        .options = context.settings->settings.options,
    };
    cover_flow_screen_init(&screens->cover_flow, cover_flow);
    for (int index = 0; index < MUSIC_ITEM_COUNT; index++) {
        screens->music_items[index] = music_item(screens, context.store, index);
    }
    app_screens_build_menu(&screens->music, i18n_text(STR_RESCAN_MUSIC_TITLE), screens->music_items,
                           MUSIC_ITEM_COUNT, context.theme);
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
        .equalizer = &screens->equalizer,
        .date_time = &screens->date_time,
        .sleep_option = 0,
    };
    DateTimeConfig date_time = {.clock = context.clock,
                                .store = context.settings,
                                .toast = context.toast,
                                .theme = context.theme};
    date_time_screen_init(&screens->date_time, date_time);
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
    return (MenuItem){
        .label = i18n_text(shelf == SPOKEN_PODCAST ? STR_MENU_PODCASTS : STR_MENU_AUDIOBOOKS),
        .select = open_browse,
        .context = entry,
        .opens = true,
        .shown = shelf_present,
        .shown_context = entry};
}

static void build_home(AppScreens *screens, AppScreensContext context)
{
    const Theme *theme = context.theme;
    screens->home_items[0] =
        app_screens_open_item(i18n_text(STR_RESCAN_MUSIC_TITLE), &screens->music.screen);
    screens->home_items[1] = shelf_item(screens, context, SPOKEN_AUDIOBOOK);
    screens->home_items[2] = shelf_item(screens, context, SPOKEN_PODCAST);
    screens->home_items[3] =
        app_screens_open_item(i18n_text(STR_MENU_SETTINGS), &screens->settings.menu.screen);
    screens->home_items[4] =
        app_screens_open_item(i18n_text(STR_DIAG_TITLE), &screens->diagnostics.screen);
    screens->home_items[5] =
        app_screens_open_item(i18n_text(STR_MENU_NOW_PLAYING), &screens->now_playing.screen);
    screens->home_items[5].shown = player_has_track;
    screens->home_items[5].shown_context = context.player;
    home_art_init(&screens->home_art, context.covers, context.store, context.player, theme);
    MenuConfig home = {.title = "EchoPod",
                       .items = screens->home_items,
                       .item_count = HOME_ITEM_COUNT,
                       .theme = theme,
                       .panel = {.width = HOME_ART_WIDTH,
                                 .min_height = HOME_ART_MIN_HEIGHT,
                                 .update = home_art_update,
                                 .draw = home_art_draw,
                                 .context = &screens->home_art}};
    menu_screen_init(&screens->home, home);
}

Screen *app_screens_init(AppScreens *screens, AppScreensContext context)
{
    const Theme *theme = context.theme;
    MessageConfig no_library = {.title = i18n_text(STR_MENU_NO_LIBRARY),
                                .message = i18n_text(STR_MENU_NO_LIBRARY_HINT),
                                .theme = theme};
    UpNextConfig up_next = {
        .library = &context.store->library, .theme = theme, .player = context.player};
    NowPlayingConfig now_playing = {.player = context.player,
                                    .library = &context.store->library,
                                    .theme = theme,
                                    .covers = context.covers,
                                    .up_next = up_next_screen_init(&screens->up_next, up_next),
                                    .options = context.settings->settings.options};
    EqualizerConfig equalizer = {
        .store = context.equalizer, .toast = context.toast, .theme = theme};
    now_playing.equalizer = equalizer_screen_init(&screens->equalizer, equalizer);
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
    if (start == START_NOW_PLAYING) {
        list_view_select(&screens->home.list, 1, 0);
        path[0] = &screens->now_playing.screen;
        return 1;
    }
    if ((int)start >= (int)START_SCREEN_COUNT || START_ENTRIES[start] == START_HOME_ONLY) {
        return 0;
    }
    return music_path(screens, START_ENTRIES[start], path);
}

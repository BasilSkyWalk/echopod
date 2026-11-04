#include "app_screens.h"

#include <stdbool.h>
#include <stddef.h>

#include "core/i18n.h"
#include "diagnostics_values.h"
#include "hal/power.h"
#include "hal/clock.h"
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
    MUSIC_COVER_FLOW_ENTRY = 0,
    MUSIC_ARTISTS_ENTRY = 1,
    MUSIC_ALBUMS_ENTRY = 3,
    MUSIC_PLAYLISTS_ENTRY = 7,
    MUSIC_FAVORITES_ENTRY = 8
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

static bool home_option_shown(const void *context)
{
    const HomeMenuOption *option = context;
    return option->available &&
           (option->was_shown == NULL || option->was_shown(option->shown_context)) &&
           (option->store->settings.main_menu_items & (UINT64_C(1) << option->bit)) != 0;
}

static const char *home_option_value(const void *context)
{
    const HomeMenuOption *option = context;
    bool enabled = (option->store->settings.main_menu_items & (UINT64_C(1) << option->bit)) != 0;
    return i18n_text(enabled ? STR_ON : STR_OFF);
}

static ScreenAction toggle_home_option(void *context)
{
    HomeMenuOption *option = context;
    uint64_t bit = UINT64_C(1) << option->bit;
    uint64_t enabled = option->store->settings.main_menu_items;
    if ((enabled & bit) != 0 && (enabled & ~bit) == 0) {
        toast_show(option->toast, i18n_text(STR_SETTINGS_KEEP_ONE_MENU), hal_clock_milliseconds());
        return screen_action_none();
    }
    option->store->settings.main_menu_items ^= bit;
    if (!settings_store_save(option->store)) {
        toast_show(option->toast, i18n_text(STR_SETTINGS_SAVE_FAILED), hal_clock_milliseconds());
    }
    return screen_action_none();
}

static void add_home_option(AppScreens *screens, int slot, MenuItem item, bool available)
{
    HomeMenuOption *option = &screens->home_options[slot];
    *option = (HomeMenuOption){.store = screens->settings.context.store,
                               .toast = screens->settings.context.toast,
                               .bit = (uint8_t)slot,
                               .available = available,
                               .was_shown = item.shown,
                               .shown_context = item.shown_context};
    screens->home_items[slot] = item;
    screens->home_items[slot].shown = home_option_shown;
    screens->home_items[slot].shown_context = option;
    if (available) {
        screens->customization_items[screens->customization_item_count++] =
            (MenuItem){.label = item.label,
                       .select = toggle_home_option,
                       .context = option,
                       .value = home_option_value};
    }
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
    return (MenuItem){.label = i18n_text(entry->label),
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
        .main_menu_customization = &screens->main_menu_customization.screen,
        .clock = &screens->clock_screen.screen,
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
    MenuItem roots[HOME_ROOT_ITEM_COUNT] = {
        app_screens_open_item(i18n_text(STR_RESCAN_MUSIC_TITLE), &screens->music.screen),
        shelf_item(screens, context, SPOKEN_AUDIOBOOK),
        shelf_item(screens, context, SPOKEN_PODCAST),
        app_screens_open_item(i18n_text(STR_MENU_SETTINGS), &screens->settings.menu.screen),
        app_screens_open_item(i18n_text(STR_DIAG_TITLE), &screens->diagnostics.screen),
        app_screens_open_item(i18n_text(STR_MENU_NOW_PLAYING), &screens->now_playing.screen),
    };
    roots[5].shown = player_has_track;
    roots[5].shown_context = context.player;
    for (int index = 0; index < HOME_ROOT_ITEM_COUNT; index++) {
        add_home_option(screens, index, roots[index], true);
    }
    int slot = HOME_ROOT_ITEM_COUNT;
    for (int index = 0; index < HOME_MUSIC_SHORTCUT_COUNT; index++) {
        add_home_option(screens, slot++, screens->music_items[index], true);
    }
    for (int index = 0; index < HOME_SETTINGS_SHORTCUT_COUNT - 1; index++) {
        add_home_option(screens, slot++, screens->settings.items[index], true);
    }
    add_home_option(screens, slot++, screens->settings.items[SETTINGS_CLOCK_ITEM], true);
    for (int index = 0; index < DIAGNOSTICS_ITEM_COUNT; index++) {
        bool available = index < screens->diagnostics.config.item_count;
        MenuItem item = available ? screens->diagnostics_items[index]
                                  : (MenuItem){.label = ""};
        add_home_option(screens, slot++, item, available);
    }
    MenuItem brick = app_screens_open_item(i18n_text(STR_MENU_BRICK), &screens->brick.screen);
    add_home_option(screens, slot++, brick, true);
    MenuItem quiz = app_screens_open_item(i18n_text(STR_MENU_MUSIC_QUIZ),
                                          &screens->music_quiz.screen);
    add_home_option(screens, slot++, quiz, true);
    home_art_init(&screens->home_art, context.covers, context.store, context.player, theme);
    MenuConfig customization = {.title = i18n_text(STR_SETTINGS_CUSTOMIZE_MAIN_MENU),
                                .items = screens->customization_items,
                                .item_count = screens->customization_item_count,
                                .theme = theme};
    menu_screen_init(&screens->main_menu_customization, customization);
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
                                    .up_next = up_next_screen_init(&screens->up_next, up_next)};
    EqualizerConfig equalizer = {
        .store = context.equalizer, .toast = context.toast, .theme = theme};
    now_playing.equalizer = equalizer_screen_init(&screens->equalizer, equalizer);
    message_screen_init(&screens->no_library, no_library);
    bluetooth_screen_init(&screens->bluetooth, context.bluetooth, theme);
    now_playing_screen_init(&screens->now_playing, now_playing);
    clock_screen_init(&screens->clock_screen, context.settings, context.toast, theme,
                      hal_clock_milliseconds());
    screen_saver_init(&screens->screen_saver, context.settings, context.player, context.covers,
                      &screens->clock_screen, theme);
    build_music(screens, context);
    build_settings(screens, context);
    diagnostics_menu_build(screens, context.options, theme);
    brick_screen_init(&screens->brick, theme);
    music_quiz_screen_init(&screens->music_quiz, context.player, theme);
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

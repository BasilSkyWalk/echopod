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

static const char *year_value(const void *context)
{
    return context;
}

static const MenuItem LONG_NAME_ITEMS[] = {
    {.label = "Sigur Rós — Ágætis byrjun", .context = "1999", .value = year_value},
    {.label = "Godspeed You! Black Emperor — Lift Your Skinny Fists Like Antennas to Heaven",
     .context = "2000",
     .value = year_value},
    {.label = "Björk — Homogenic", .context = "1997", .value = year_value},
    {.label = "Sufjan Stevens — Illinois (Come On Feel the Illinoise)",
     .context = "2005",
     .value = year_value},
    {.label = "Motörhead — Ace of Spades", .context = "1980", .value = year_value},
    {.label = "Jóhann Jóhannsson — Orphée", .context = "2016", .value = year_value},
    {.label = "Ólafur Arnalds — re:member", .context = "2018", .value = year_value},
    {.label = "Röyksopp — Melody A.M.", .context = "2001", .value = year_value},
    {.label = "Café Tacvba — Re", .context = "1994", .value = year_value},
    {.label = "Beyoncé — Lemonade", .context = "2016", .value = year_value},
    {.label = "Mötley Crüe — Dr. Feelgood", .context = "1989", .value = year_value},
    {.label = "Daft Punk — Random Access Memories", .context = "2013", .value = year_value},
    {.label = "Explosions in the Sky — The Earth Is Not a Cold Dead Place",
     .context = "2003",
     .value = year_value},
    {.label = "Hania Rani — Esja", .context = "2019", .value = year_value},
    {.label = "Nils Frahm — All Melody", .context = "2018", .value = year_value},
    {.label = "The Avalanches — Since I Left You", .context = "2000", .value = year_value},
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

static ScreenAction toggle_power_taps(void *context)
{
    AppOptions *options = context;
    options->ignore_power_taps = !options->ignore_power_taps;
    return screen_action_none();
}

static const char *power_taps_value(const void *context)
{
    const AppOptions *options = context;
    return options->ignore_power_taps ? "Ignored" : "Normal";
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

static char high_ram_text[DIAGNOSTICS_VALUE_CAPACITY];

static ScreenAction run_high_ram_test(void *context)
{
    HighRamOption *option = context;
    option->test = hal_system_high_ram_test();
    option->run = true;
    return screen_action_none();
}

static const char *high_ram_value(const void *context)
{
    const HighRamOption *option = context;
    diagnostics_high_ram_text(option->run, option->test, high_ram_text, sizeof high_ram_text);
    return high_ram_text;
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

static char core1_text[DIAGNOSTICS_VALUE_CAPACITY];

static ScreenAction run_core1_test(void *context)
{
    Core1Option *core1 = context;
    core1->test = hal_system_core1_test();
    core1->run = true;
    return screen_action_none();
}

static const char *core1_value(const void *context)
{
    const Core1Option *core1 = context;
    diagnostics_core1_text(core1->run, core1->test, core1_text, sizeof core1_text);
    return core1_text;
}

static char core1_accel_text[DIAGNOSTICS_VALUE_CAPACITY];

static ScreenAction run_core1_accel_test(void *context)
{
    Core1AccelOption *accel = context;
    accel->result = hal_system_core1_accel_test();
    accel->run = true;
    return screen_action_none();
}

static const char *core1_accel_value(const void *context)
{
    const Core1AccelOption *accel = context;
    diagnostics_core1_accel_text(accel->run, accel->result, core1_accel_text,
                                 sizeof core1_accel_text);
    return core1_accel_text;
}

static char core1_decode_text[DIAGNOSTICS_VALUE_CAPACITY];

static ScreenAction run_core1_decode_test(void *context)
{
    Core1DecodeOption *decode = context;
    decode->result = hal_system_core1_decode_test();
    decode->run = true;
    return screen_action_none();
}

static const char *core1_decode_value(const void *context)
{
    const Core1DecodeOption *decode = context;
    diagnostics_core1_decode_text(decode->run, decode->result, core1_decode_text,
                                  sizeof core1_decode_text);
    return core1_decode_text;
}

static char battery_text[DIAGNOSTICS_VALUE_CAPACITY];

static const char *battery_value(const void *context)
{
    (void)context;
    diagnostics_battery_text(hal_power_battery(), battery_text, sizeof battery_text);
    return battery_text;
}

static char boot_hold_text[DIAGNOSTICS_VALUE_CAPACITY];

static const char *boot_hold_value(const void *context)
{
    (void)context;
    diagnostics_boot_hold_text(hal_power_boot_hold(), boot_hold_text, sizeof boot_hold_text);
    return boot_hold_text;
}

static char jack_single_ended_text[DIAGNOSTICS_VALUE_CAPACITY];
static char jack_balanced_text[DIAGNOSTICS_VALUE_CAPACITY];

static const char *jack_single_ended_value(const void *context)
{
    (void)context;
    diagnostics_jack_pin_text(hal_audio_jack_pins().single_ended, jack_single_ended_text,
                              sizeof jack_single_ended_text);
    return jack_single_ended_text;
}

static const char *jack_balanced_value(const void *context)
{
    (void)context;
    diagnostics_jack_pin_text(hal_audio_jack_pins().balanced, jack_balanced_text,
                              sizeof jack_balanced_text);
    return jack_balanced_text;
}

static char start_text[DIAGNOSTICS_VALUE_CAPACITY];

static const char *start_value(const void *context)
{
    (void)context;
    diagnostics_start_text(hal_system_start_info(), start_text, sizeof start_text);
    return start_text;
}

static char last_step_text[DIAGNOSTICS_VALUE_CAPACITY];

static const char *last_step_value(const void *context)
{
    (void)context;
    diagnostics_last_step_text(hal_system_start_info(), last_step_text, sizeof last_step_text);
    return last_step_text;
}

static char last_battery_text[DIAGNOSTICS_VALUE_CAPACITY];

static const char *last_battery_value(const void *context)
{
    (void)context;
    diagnostics_last_battery_text(hal_system_start_info(), last_battery_text,
                                  sizeof last_battery_text);
    return last_battery_text;
}

static ScreenAction run_stall_test(void *context)
{
    (void)context;
    hal_system_stall_test();
    return screen_action_none();
}

static ScreenAction run_watchdog_test(void *context)
{
    (void)context;
    hal_system_watchdog_test();
    return screen_action_none();
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

static char usb_config_text[DIAGNOSTICS_VALUE_CAPACITY];
static char usb_phy_text[DIAGNOSTICS_VALUE_CAPACITY];
static char usb_fifo_text[DIAGNOSTICS_VALUE_CAPACITY];

static const char *usb_config_value(const void *context)
{
    (void)context;
    HalUsbCore core = hal_system_usb_core();
    diagnostics_usb_registers_text(core.captured, core.hwcfg2, core.hwcfg3, usb_config_text,
                                   sizeof usb_config_text);
    return usb_config_text;
}

static const char *usb_phy_value(const void *context)
{
    (void)context;
    HalUsbCore core = hal_system_usb_core();
    diagnostics_usb_registers_text(core.captured, core.hwcfg4, core.usbcfg, usb_phy_text,
                                   sizeof usb_phy_text);
    return usb_phy_text;
}

static const char *usb_fifo_value(const void *context)
{
    (void)context;
    HalUsbCore core = hal_system_usb_core();
    diagnostics_usb_registers_text(core.captured, core.rx_fifo, core.nptx_fifo, usb_fifo_text,
                                   sizeof usb_fifo_text);
    return usb_fifo_text;
}

static char usb_last_text[DIAGNOSTICS_VALUE_CAPACITY];

static const char *usb_last_value(const void *context)
{
    (void)context;
    diagnostics_usb_last_text(hal_system_usb_last(), usb_last_text, sizeof usb_last_text);
    return usb_last_text;
}

static ScreenAction toggle_dma_tone(void *context)
{
    DmaToneOption *tone = context;
    if (hal_audio_dma_tone_status().running) {
        hal_audio_dma_tone_stop();
        tone->stopped = true;
        return screen_action_none();
    }
    tone->result = hal_audio_dma_tone_start(tone->sample_rate);
    tone->tried = true;
    tone->stopped = false;
    return screen_action_none();
}

enum {
    LOW_TONE_RATE = 44100,
    HIGH_TONE_RATE = 192000,
    TONE_176_RATE = 176400
};

static char dma_tone_text[DIAGNOSTICS_VALUE_CAPACITY];
static char high_rate_tone_text[DIAGNOSTICS_VALUE_CAPACITY];
static char tone_176_text[DIAGNOSTICS_VALUE_CAPACITY];

static char *tone_text_for(uint32_t sample_rate)
{
    switch (sample_rate) {
    case HIGH_TONE_RATE:
        return high_rate_tone_text;
    case TONE_176_RATE:
        return tone_176_text;
    default:
        return dma_tone_text;
    }
}

static const char *dma_tone_value(const void *context)
{
    const DmaToneOption *tone = context;
    char *text = tone_text_for(tone->sample_rate);
    DmaToneView view = {.tried = tone->tried,
                        .stopped = tone->stopped,
                        .result = tone->result,
                        .status = hal_audio_dma_tone_status(),
                        .detail = hal_audio_tone_detail()};
    diagnostics_dma_tone_text(view, text, DIAGNOSTICS_VALUE_CAPACITY);
    return text;
}

static bool player_has_track(const void *context)
{
    return player_status(context).has_track;
}

static MenuItem open_item(const char *label, Screen *target)
{
    return (MenuItem){.label = label,
                      .select = menu_open_screen,
                      .context = target,
                      .value = NULL,
                      .opens = true};
}

static Screen *build_menu(MenuScreen *menu, const char *title, const MenuItem *items, int count,
                          const Theme *theme)
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
        .rescan_requested = context.rescan_requested,
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
    screens->dma_tone = (DmaToneOption){
        .sample_rate = LOW_TONE_RATE, .tried = false, .stopped = false, .result = HAL_TONE_PLAYED};
    screens->high_rate_tone = (DmaToneOption){
        .sample_rate = HIGH_TONE_RATE, .tried = false, .stopped = false, .result = HAL_TONE_PLAYED};
    screens->tone_176 = (DmaToneOption){
        .sample_rate = TONE_176_RATE, .tried = false, .stopped = false, .result = HAL_TONE_PLAYED};
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
    screens->core1 = (Core1Option){.run = false};
    screens->diagnostics_items[12] = (MenuItem){.label = "Core 1",
                                                .select = run_core1_test,
                                                .context = &screens->core1,
                                                .value = core1_value};
    screens->core1_decode = (Core1DecodeOption){.run = false};
    screens->diagnostics_items[13] = (MenuItem){.label = "Core 1 decode",
                                                .select = run_core1_decode_test,
                                                .context = &screens->core1_decode,
                                                .value = core1_decode_value};
    screens->diagnostics_items[14] =
        (MenuItem){.label = "Battery", .select = NULL, .context = NULL, .value = battery_value};
    screens->diagnostics_items[15] =
        (MenuItem){.label = "Boot hold", .select = NULL, .context = NULL, .value = boot_hold_value};
    screens->diagnostics_items[16] =
        (MenuItem){.label = "Last start", .select = NULL, .context = NULL, .value = start_value};
    screens->diagnostics_items[17] =
        (MenuItem){.label = "Last step", .select = NULL, .context = NULL, .value = last_step_value};
    screens->diagnostics_items[18] = (MenuItem){
        .label = "Last battery", .select = NULL, .context = NULL, .value = last_battery_value};
    screens->diagnostics_items[19] =
        open_item("Bluetooth", bluetooth_screen_init(&screens->bluetooth, theme));
    screens->diagnostics_items[20] =
        (MenuItem){.label = "Stall test", .select = run_stall_test, .context = NULL, .value = NULL};
    screens->high_ram = (HighRamOption){.run = false};
    screens->diagnostics_items[22] = (MenuItem){.label = "High RAM",
                                                .select = run_high_ram_test,
                                                .context = &screens->high_ram,
                                                .value = high_ram_value};
    screens->diagnostics_items[21] = (MenuItem){.label = "Power taps",
                                                .select = toggle_power_taps,
                                                .context = options,
                                                .value = power_taps_value};
    screens->diagnostics_items[23] = (MenuItem){
        .label = "Jack 3.5", .select = NULL, .context = NULL, .value = jack_single_ended_value};
    screens->diagnostics_items[24] = (MenuItem){
        .label = "Jack 4.4", .select = NULL, .context = NULL, .value = jack_balanced_value};
    screens->core1_accel = (Core1AccelOption){.run = false};
    screens->diagnostics_items[25] = (MenuItem){.label = "LPC accel",
                                                .select = run_core1_accel_test,
                                                .context = &screens->core1_accel,
                                                .value = core1_accel_value};
    screens->diagnostics_items[26] = (MenuItem){.label = "Tone 192k",
                                                .select = toggle_dma_tone,
                                                .context = &screens->high_rate_tone,
                                                .value = dma_tone_value};
    screens->diagnostics_items[27] = (MenuItem){
        .label = "Watchdog test", .select = run_watchdog_test, .context = NULL, .value = NULL};
    screens->diagnostics_items[28] = (MenuItem){.label = "Tone 176k",
                                                .select = toggle_dma_tone,
                                                .context = &screens->tone_176,
                                                .value = dma_tone_value};
    screens->diagnostics_items[29] = (MenuItem){
        .label = "USB HW2 HW3", .select = NULL, .context = NULL, .value = usb_config_value};
    screens->diagnostics_items[30] =
        (MenuItem){.label = "USB HW4 CFG", .select = NULL, .context = NULL, .value = usb_phy_value};
    screens->diagnostics_items[31] =
        (MenuItem){.label = "USB RX TX", .select = NULL, .context = NULL, .value = usb_fifo_value};
    screens->diagnostics_items[32] =
        (MenuItem){.label = "Last USB", .select = NULL, .context = NULL, .value = usb_last_value};
    build_menu(&screens->diagnostics, "Diagnostics", screens->diagnostics_items,
               DIAGNOSTICS_ITEM_COUNT, theme);
}

static void build_home(AppScreens *screens, AppScreensContext context)
{
    const Theme *theme = context.theme;
    screens->home_items[0] = open_item("Music", &screens->music.screen);
    screens->home_items[1] = open_item("Settings", &screens->settings.menu.screen);
    screens->home_items[2] = open_item("Diagnostics", &screens->diagnostics.screen);
    screens->home_items[3] = open_item("Now Playing", &screens->now_playing.screen);
    screens->home_items[3].shown = player_has_track;
    screens->home_items[3].shown_context = context.player;
    home_art_init(&screens->home_art, context.covers, context.store, context.player, theme);
    MenuConfig home = {.title = "Echo Mini",
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
    now_playing_screen_init(&screens->now_playing, now_playing);
    build_music(screens, context);
    build_settings(screens, context);
    build_diagnostics(screens, context.options, theme);
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

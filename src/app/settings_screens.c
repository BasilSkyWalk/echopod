#include "settings_screens.h"

#include "core/i18n.h"
#include "core/string_builder.h"
#include "hal/audio.h"
#include "hal/clock.h"
#include "hal/storage.h"
#include "hal/system.h"
#include "library/library.h"

enum {
    BRIGHTNESS_LEVEL_COUNT = SETTINGS_BRIGHTNESS_MAX - SETTINGS_BRIGHTNESS_MIN + 1,
    MILLISECONDS_PER_MINUTE = 60000
};

static const StringId SHUFFLE_OPTIONS[SHUFFLE_MODE_COUNT] = {STR_OFF, STR_SETTINGS_SONGS,
                                                             STR_MENU_ALBUMS};
static const StringId REPEAT_OPTIONS[REPEAT_MODE_COUNT] = {STR_OFF, STR_SETTINGS_ONE,
                                                           STR_SETTINGS_ALL};
static const StringId SLEEP_OPTIONS[SLEEP_TIMER_OPTION_COUNT] = {STR_OFF,
                                                                 STR_SETTINGS_MINUTES_15,
                                                                 STR_SETTINGS_MINUTES_30,
                                                                 STR_SETTINGS_MINUTES_45,
                                                                 STR_SETTINGS_MINUTES_60,
                                                                 STR_SETTINGS_MINUTES_90,
                                                                 STR_SETTINGS_MINUTES_120};
static const StringId SLEEP_MESSAGES[SLEEP_TIMER_OPTION_COUNT] = {
    STR_SETTINGS_SLEEP_OFF,   STR_SETTINGS_SLEEP_IN_15, STR_SETTINGS_SLEEP_IN_30,
    STR_SETTINGS_SLEEP_IN_45, STR_SETTINGS_SLEEP_IN_60, STR_SETTINGS_SLEEP_IN_90,
    STR_SETTINGS_SLEEP_IN_120};
static const StringId AUTO_OFF_OPTIONS[AUTO_OFF_OPTION_COUNT] = {
    STR_OFF, STR_SETTINGS_MINUTES_1, STR_SETTINGS_MINUTES_5, STR_SETTINGS_MINUTES_10};
static const StringId TIMEOUT_OPTIONS[SCREEN_TIMEOUT_OPTION_COUNT] = {
    STR_SETTINGS_SECONDS_10, STR_SETTINGS_SECONDS_20, STR_SETTINGS_SECONDS_30,
    STR_SETTINGS_MINUTES_1,  STR_SETTINGS_MINUTES_2,  STR_SETTINGS_MINUTES_5,
    STR_SETTINGS_NEVER};
static const char *const BRIGHTNESS_OPTIONS[BRIGHTNESS_LEVEL_COUNT] = {"1", "2", "3", "4", "5",
                                                                       "6", "7", "8", "9", "10"};
static const StringId ORIENTATION_OPTIONS[ORIENTATION_COUNT] = {
    STR_SETTINGS_LANDSCAPE, STR_SETTINGS_PORTRAIT, STR_SETTINGS_PORTRAIT_FLIPPED};
static const StringId GAIN_OPTIONS[GAIN_LEVEL_COUNT] = {STR_SETTINGS_LOW, STR_SETTINGS_HIGH};
static const char *const FILTER_OPTIONS[DAC_FILTER_COUNT] = {"1", "2", "3", "4", "5"};
static const char *const BALANCE_OPTIONS[SETTINGS_BALANCE_STEPS] = {
    "L12", "L11", "L10", "L9", "L8", "L7", "L6", "L5", "L4", "L3",  "L2",  "L1", "0",
    "R1",  "R2",  "R3",  "R4", "R5", "R6", "R7", "R8", "R9", "R10", "R11", "R12"};
static const StringId OUTPUT_OPTIONS[OUTPUT_JACK_COUNT] = {STR_SETTINGS_AUTO, STR_SETTINGS_JACK_35,
                                                           STR_SETTINGS_JACK_44};
static const StringId OUTPUT_MESSAGES[OUTPUT_JACK_COUNT] = {
    STR_SETTINGS_OUTPUT_FOLLOWS, STR_SETTINGS_OUTPUT_USING_35, STR_SETTINGS_OUTPUT_USING_44};
static const StringId START_OPTIONS[START_SCREEN_COUNT] = {
    STR_SETTINGS_MAIN_MENU, STR_MENU_NOW_PLAYING, STR_MENU_ALBUMS,    STR_MENU_ARTISTS,
    STR_MENU_FAVORITES,     STR_MENU_PLAYLISTS,   STR_MENU_COVER_FLOW};
static const StringId LANGUAGE_OPTIONS[LANGUAGE_COUNT] = {
    STR_LANGUAGE_EN, STR_LANGUAGE_ZH, STR_LANGUAGE_JA, STR_LANGUAGE_KO, STR_LANGUAGE_DE,
    STR_LANGUAGE_FR, STR_LANGUAGE_ES, STR_LANGUAGE_IT, STR_LANGUAGE_PT, STR_LANGUAGE_NL,
    STR_LANGUAGE_SV, STR_LANGUAGE_DA, STR_LANGUAGE_NO, STR_LANGUAGE_FI};
static const StringId USB_MODE_OPTIONS[USB_MODE_COUNT] = {STR_SETTINGS_STORAGE, STR_SETTINGS_DAC};
static const StringId USB_MODE_MESSAGES[USB_MODE_COUNT] = {STR_SETTINGS_USB_DRIVES,
                                                           STR_SETTINGS_USB_SOUND};

static const Settings *settings_of(const void *context)
{
    const SettingsContext *settings = context;
    return &settings->store->settings;
}

static Settings *editable_settings(void *context)
{
    SettingsContext *settings = context;
    return &settings->store->settings;
}

static void save(SettingsContext *context)
{
    settings_store_capture(context->store, player_status(context->player));
    if (!settings_store_save(context->store)) {
        toast_show(context->toast, i18n_text(STR_SETTINGS_SAVE_FAILED), hal_clock_milliseconds());
    }
}

static void apply_and_save(SettingsContext *context)
{
    settings_store_apply_hardware(&context->store->settings);
    save(context);
}

static int current_shuffle(const void *context)
{
    const SettingsContext *settings = context;
    return (int)player_status(settings->player).shuffle;
}

static void choose_shuffle(void *context, int index)
{
    SettingsContext *settings = context;
    player_set_shuffle(settings->player, (ShuffleMode)index, hal_clock_microseconds());
    save(settings);
}

static int current_repeat(const void *context)
{
    const SettingsContext *settings = context;
    return (int)player_status(settings->player).repeat;
}

static void choose_repeat(void *context, int index)
{
    SettingsContext *settings = context;
    player_set_repeat(settings->player, (RepeatMode)index);
    save(settings);
}

static int current_sleep(const void *context)
{
    const SettingsContext *settings = context;
    return settings->power->sleep_timer_active ? settings->sleep_option : 0;
}

static void choose_sleep(void *context, int index)
{
    SettingsContext *settings = context;
    uint32_t now_ms = hal_clock_milliseconds();
    uint32_t minutes = settings_sleep_option_minutes(index);
    settings->sleep_option = index;
    if (minutes == 0) {
        power_policy_cancel_sleep_timer(settings->power);
    } else {
        power_policy_start_sleep_timer(settings->power, minutes * MILLISECONDS_PER_MINUTE, now_ms);
    }
    toast_show(settings->toast, i18n_text(SLEEP_MESSAGES[index]), now_ms);
}

static int current_timeout(const void *context)
{
    return settings_of(context)->screen_timeout;
}

static void choose_timeout(void *context, int index)
{
    SettingsContext *settings = context;
    editable_settings(context)->screen_timeout = (uint8_t)index;
    power_policy_set_timeout(settings->power, settings_screen_timeout_ms(settings_of(context)));
    save(settings);
}

static int current_auto_off(const void *context)
{
    return settings_of(context)->auto_off;
}

static void choose_auto_off(void *context, int index)
{
    SettingsContext *settings = context;
    editable_settings(context)->auto_off = (uint8_t)index;
    power_policy_set_auto_off(settings->power, settings_auto_off_ms(settings_of(context)),
                              hal_clock_milliseconds());
    save(settings);
}

static int current_brightness(const void *context)
{
    return settings_of(context)->brightness - SETTINGS_BRIGHTNESS_MIN;
}

static void choose_brightness(void *context, int index)
{
    editable_settings(context)->brightness = (uint8_t)(index + SETTINGS_BRIGHTNESS_MIN);
    apply_and_save(context);
}

static int current_orientation(const void *context)
{
    return (int)settings_of(context)->orientation;
}

static void choose_orientation(void *context, int index)
{
    editable_settings(context)->orientation = (Orientation)index;
    save(context);
}

static int current_gain(const void *context)
{
    return (int)settings_of(context)->gain;
}

static void choose_gain(void *context, int index)
{
    editable_settings(context)->gain = (GainLevel)index;
    apply_and_save(context);
}

static int current_filter(const void *context)
{
    return settings_of(context)->filter;
}

static void choose_filter(void *context, int index)
{
    editable_settings(context)->filter = (uint8_t)index;
    apply_and_save(context);
}

static int current_balance(const void *context)
{
    return settings_of(context)->balance;
}

static void choose_balance(void *context, int index)
{
    editable_settings(context)->balance = (uint8_t)index;
    apply_and_save(context);
}

static int current_output(const void *context)
{
    return (int)settings_of(context)->output;
}

static void choose_output(void *context, int index)
{
    SettingsContext *settings = context;
    editable_settings(context)->output = (OutputJack)index;
    apply_and_save(context);
    toast_show(settings->toast, i18n_text(OUTPUT_MESSAGES[index]), hal_clock_milliseconds());
}

static const char *describe_output(int current)
{
    HalAudioJack jack = hal_audio_jack();
    if (current != OUTPUT_AUTO || jack == HAL_AUDIO_JACK_NONE) {
        return i18n_text(OUTPUT_OPTIONS[current]);
    }
    return i18n_text(jack == HAL_AUDIO_JACK_BALANCED ? STR_SETTINGS_AUTO_44 : STR_SETTINGS_AUTO_35);
}

static int current_start(const void *context)
{
    return (int)settings_of(context)->start_screen;
}

static void choose_start(void *context, int index)
{
    editable_settings(context)->start_screen = (StartScreen)index;
    save(context);
}

static int current_usb_mode(const void *context)
{
    return (int)settings_of(context)->usb_mode;
}

static void choose_usb_mode(void *context, int index)
{
    SettingsContext *settings = context;
    editable_settings(context)->usb_mode = (UsbMode)index;
    apply_and_save(context);
    toast_show(settings->toast, i18n_text(USB_MODE_MESSAGES[index]), hal_clock_milliseconds());
}

static int current_language(const void *context)
{
    return (int)settings_of(context)->language;
}

static void choose_language(void *context, int index)
{
    SettingsContext *settings = context;
    editable_settings(context)->language = (Language)index;
    save(context);
    toast_show(settings->toast, i18n_text(STR_LANGUAGE_RESTART), hal_clock_milliseconds());
}

static const SettingChoice CHOICES[SETTINGS_CHOICE_COUNT] = {
    {STR_SETTINGS_SHUFFLE, SHUFFLE_OPTIONS, NULL, SHUFFLE_MODE_COUNT, current_shuffle,
     choose_shuffle, NULL},
    {STR_SETTINGS_REPEAT, REPEAT_OPTIONS, NULL, REPEAT_MODE_COUNT, current_repeat, choose_repeat,
     NULL},
    {STR_SETTINGS_SLEEP_TIMER, SLEEP_OPTIONS, NULL, SLEEP_TIMER_OPTION_COUNT, current_sleep,
     choose_sleep, NULL},
    {STR_SETTINGS_AUTO_OFF, AUTO_OFF_OPTIONS, NULL, AUTO_OFF_OPTION_COUNT, current_auto_off,
     choose_auto_off, NULL},
    {STR_SETTINGS_SCREEN_TIMEOUT, TIMEOUT_OPTIONS, NULL, SCREEN_TIMEOUT_OPTION_COUNT,
     current_timeout, choose_timeout, NULL},
    {STR_SETTINGS_BRIGHTNESS, NULL, BRIGHTNESS_OPTIONS, BRIGHTNESS_LEVEL_COUNT, current_brightness,
     choose_brightness, NULL},
    {STR_SETTINGS_ORIENTATION, ORIENTATION_OPTIONS, NULL, ORIENTATION_COUNT, current_orientation,
     choose_orientation, NULL},
    {STR_SETTINGS_GAIN, GAIN_OPTIONS, NULL, GAIN_LEVEL_COUNT, current_gain, choose_gain, NULL},
    {STR_SETTINGS_FILTER, NULL, FILTER_OPTIONS, DAC_FILTER_COUNT, current_filter, choose_filter,
     NULL},
    {STR_SETTINGS_BALANCE, NULL, BALANCE_OPTIONS, SETTINGS_BALANCE_STEPS, current_balance,
     choose_balance, NULL},
    {STR_SETTINGS_OUTPUT, OUTPUT_OPTIONS, NULL, OUTPUT_JACK_COUNT, current_output, choose_output,
     describe_output},
    {STR_SETTINGS_START_SCREEN, START_OPTIONS, NULL, START_SCREEN_COUNT, current_start,
     choose_start, NULL},
    {STR_SETTINGS_USB_MODE, USB_MODE_OPTIONS, NULL, USB_MODE_COUNT, current_usb_mode,
     choose_usb_mode, NULL},
    {STR_LANGUAGE_TITLE, LANGUAGE_OPTIONS, NULL, LANGUAGE_COUNT, current_language, choose_language,
     NULL},
};

static ScreenAction open_choice(void *context)
{
    SettingsEntry *entry = context;
    SettingsScreens *screens = entry->owner;
    const SettingChoice *choice = entry->choice;
    ChoiceConfig config = {
        .title = i18n_text(choice->title),
        .options = choice->plain_options,
        .option_ids = choice->options,
        .option_count = choice->option_count,
        .current = choice->current,
        .choose = choice->choose,
        .context = &screens->context,
        .theme = screens->theme,
    };
    return screen_action_push(choice_screen_init(&screens->choice, config));
}

static const char *choice_value(const void *context)
{
    const SettingsEntry *entry = context;
    int current = entry->choice->current(&entry->owner->context);
    if (current < 0 || current >= entry->choice->option_count) {
        return "";
    }
    if (entry->choice->describe != NULL) {
        return entry->choice->describe(current);
    }
    if (entry->choice->options != NULL) {
        return i18n_text(entry->choice->options[current]);
    }
    return entry->choice->plain_options[current];
}

static ScreenAction request_rescan(void *context)
{
    SettingsContext *settings = context;
    if (settings->rescan_requested != NULL) {
        *settings->rescan_requested = true;
    }
    return screen_action_none();
}

static void format_size(char *text, size_t capacity, uint64_t bytes)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (bytes >= 1000000000u) {
        uint32_t tenths = (uint32_t)((bytes + 50000000u) / 100000000u);
        string_builder_append_unsigned(&builder, tenths / 10u);
        string_builder_append_char(&builder, '.');
        string_builder_append_unsigned(&builder, tenths % 10u);
        string_builder_append(&builder, " GB");
    } else {
        string_builder_append_unsigned(&builder, (uint32_t)((bytes + 500000u) / 1000000u));
        string_builder_append(&builder, " MB");
    }
}

static char about_capacity[16];
static char about_available[16];
static char about_songs[16];

static const char *about_capacity_value(const void *context)
{
    (void)context;
    HalStorageInfo info = hal_storage_info();
    if (info.capacity_bytes == 0) {
        return i18n_text(STR_ABOUT_UNKNOWN);
    }
    format_size(about_capacity, sizeof about_capacity, info.capacity_bytes);
    return about_capacity;
}

static const char *about_available_value(const void *context)
{
    (void)context;
    HalStorageInfo info = hal_storage_info();
    if (!info.available_known) {
        return i18n_text(STR_ABOUT_UNKNOWN);
    }
    format_size(about_available, sizeof about_available, info.available_bytes);
    return about_available;
}

static const char *about_songs_value(const void *context)
{
    const SettingsScreens *screens = context;
    const Library *library = screens->context.player->library;
    if (library == NULL) {
        return i18n_text(STR_ABOUT_UNKNOWN);
    }
    StringBuilder builder = string_builder_make(about_songs, sizeof about_songs);
    string_builder_append_unsigned(&builder, library_count(library, LIBRARY_TRACKS));
    return about_songs;
}

static const char *about_firmware_value(const void *context)
{
    (void)context;
    return hal_system_version();
}

Screen *settings_screens_init(SettingsScreens *screens, SettingsContext context, const Theme *theme)
{
    screens->context = context;
    screens->theme = theme;
    screens->items[SETTINGS_RESCAN_ITEM] = (MenuItem){.label = i18n_text(STR_SETTINGS_RESCAN),
                                                      .select = request_rescan,
                                                      .context = &screens->context};
    screens->items[SETTINGS_BLUETOOTH_ITEM] = (MenuItem){.label = i18n_text(STR_BT_TITLE),
                                                         .select = bluetooth_screen_open,
                                                         .context = context.bluetooth,
                                                         .value = bluetooth_screen_summary,
                                                         .opens = true};
    screens->items[SETTINGS_EQUALIZER_ITEM] = (MenuItem){.label = i18n_text(STR_SETTINGS_EQUALIZER),
                                                         .select = equalizer_screen_open,
                                                         .context = context.equalizer,
                                                         .value = equalizer_screen_summary,
                                                         .opens = true};
    screens->about_items[0] = (MenuItem){.label = i18n_text(STR_ABOUT_SD_CARD),
                                         .value = about_capacity_value};
    screens->about_items[1] = (MenuItem){.label = i18n_text(STR_ABOUT_AVAILABLE),
                                         .value = about_available_value};
    screens->about_items[2] = (MenuItem){.label = i18n_text(STR_ABOUT_SONGS),
                                         .value = about_songs_value,
                                         .context = screens};
    screens->about_items[3] = (MenuItem){.label = i18n_text(STR_ABOUT_FIRMWARE),
                                         .value = about_firmware_value};
    MenuConfig about = {.title = i18n_text(STR_SETTINGS_ABOUT),
                        .items = screens->about_items,
                        .item_count = ABOUT_ITEM_COUNT,
                        .theme = theme};
    menu_screen_init(&screens->about, about);
    screens->items[SETTINGS_ABOUT_ITEM] = (MenuItem){.label = i18n_text(STR_SETTINGS_ABOUT),
                                                     .select = menu_open_screen,
                                                     .context = &screens->about,
                                                     .opens = true};
    for (int index = 0; index < SETTINGS_CHOICE_COUNT; index++) {
        screens->entries[index] = (SettingsEntry){.choice = &CHOICES[index], .owner = screens};
        int item = index < SETTINGS_BLUETOOTH_ITEM ? index : index + 1;
        item = item < SETTINGS_EQUALIZER_ITEM ? item : item + 1;
        item = item < SETTINGS_RESCAN_ITEM ? item : item + 1;
        screens->items[item] = (MenuItem){.label = i18n_text(CHOICES[index].title),
                                          .select = open_choice,
                                          .context = &screens->entries[index],
                                          .value = choice_value,
                                          .opens = true};
    }
    MenuConfig menu = {.title = i18n_text(STR_MENU_SETTINGS),
                       .items = screens->items,
                       .item_count = SETTINGS_ITEM_COUNT,
                       .theme = theme};
    return menu_screen_init(&screens->menu, menu);
}

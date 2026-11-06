#include "settings_screens.h"

#include "core/i18n.h"
#include "core/resident.h"
#include "hal/audio.h"
#include "hal/clock.h"

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
    STR_SETTINGS_MAIN_MENU, STR_MENU_NOW_PLAYING, STR_MENU_ALBUMS,     STR_MENU_ARTISTS,
    STR_MENU_FAVORITES,     STR_MENU_PLAYLISTS,   STR_MENU_COVER_FLOW, STR_MENU_ALBUM_ARTISTS,
    STR_MENU_GENRES,        STR_MENU_FOLDERS,     STR_MENU_ALL_SONGS};
static const StringId LANGUAGE_OPTIONS[LANGUAGE_COUNT] = {
    STR_LANGUAGE_EN, STR_LANGUAGE_ZH, STR_LANGUAGE_JA, STR_LANGUAGE_KO, STR_LANGUAGE_DE,
    STR_LANGUAGE_FR, STR_LANGUAGE_ES, STR_LANGUAGE_IT, STR_LANGUAGE_PT, STR_LANGUAGE_NL,
    STR_LANGUAGE_SV, STR_LANGUAGE_DA, STR_LANGUAGE_NO, STR_LANGUAGE_FI, STR_LANGUAGE_RU,
    STR_LANGUAGE_UK, STR_LANGUAGE_PL, STR_LANGUAGE_ID};
static const StringId SHOWN_OPTIONS[2] = {STR_ON, STR_OFF};
static const StringId SWITCH_OPTIONS[2] = {STR_OFF, STR_ON};
static const StringId ROCKER_OPTIONS[ROCKER_MODE_COUNT] = {STR_SETTINGS_ROCKER_VOLUME_SKIP,
                                                           STR_SETTINGS_ROCKER_SKIP_VOLUME,
                                                           STR_SETTINGS_ROCKER_VOLUME_SEEK};
static const StringId MENU_KEY_OPTIONS[2] = {STR_MENU_UP_NEXT, STR_SETTINGS_NEXT_TRACK};
static const StringId ACCENT_OPTIONS[ACCENT_COUNT] = {
    STR_SETTINGS_COLOR_BLUE,  STR_SETTINGS_COLOR_RED,     STR_SETTINGS_COLOR_ORANGE,
    STR_SETTINGS_COLOR_GREEN, STR_SETTINGS_COLOR_TEAL,    STR_SETTINGS_COLOR_PURPLE,
    STR_SETTINGS_COLOR_PINK,  STR_SETTINGS_COLOR_GRAPHITE};
static const StringId HIGHLIGHT_OPTIONS[2] = {STR_SETTINGS_GRADIENT, STR_SETTINGS_FLAT};
static const StringId COVER_ORDER_OPTIONS[2] = {STR_SETTINGS_BY_ALBUM_ARTIST,
                                                STR_SETTINGS_BY_ALBUM_TITLE};
static const StringId ARTIST_VIEW_OPTIONS[2] = {STR_SETTINGS_LIST, STR_MENU_COVER_FLOW};
static const StringId BLUETOOTH_LEVEL_OPTIONS[2] = {STR_SETTINGS_STANDARD, STR_SETTINGS_LOUDER};
static const StringId COVER_COUNT_OPTIONS[2] = {STR_SETTINGS_FIVE_COVERS,
                                                STR_SETTINGS_THREE_COVERS};
static const StringId ANIMATION_OPTIONS[ANIMATION_SPEED_COUNT] = {
    STR_SETTINGS_NORMAL, STR_SETTINGS_SLOWER, STR_SETTINGS_FASTER};
static const StringId THEME_OPTIONS[2] = {STR_SETTINGS_DARK, STR_SETTINGS_LIGHT};
static const StringId STATUS_BAR_OPTIONS[2] = {STR_SETTINGS_STANDARD, STR_SETTINGS_LARGE};
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

static int current_bluetooth_level(const void *context)
{
    return settings_of(context)->options[OPTION_BLUETOOTH_LEVEL];
}

static void choose_bluetooth_level(void *context, int index)
{
    editable_settings(context)->options[OPTION_BLUETOOTH_LEVEL] = (uint8_t)index;
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

static uint8_t *option_of(const SettingsEntry *entry)
{
    return &entry->owner->context.store->settings.options[entry->choice->slot - 1];
}

static int current_slot(const void *context)
{
    return *option_of(context);
}

static void choose_slot(void *context, int index)
{
    SettingsEntry *entry = context;
    SettingsContext *settings = &entry->owner->context;
    int option = entry->choice->slot - 1;
    bool changed = *option_of(entry) != (uint8_t)index;
    *option_of(entry) = (uint8_t)index;
    if (option == OPTION_INTERNAL_STORAGE && changed && settings->rescan_requested != NULL) {
        *settings->rescan_requested = true;
    }
    if (option == OPTION_USB_PROMPT || option == OPTION_INTERNAL_STORAGE) {
        apply_and_save(settings);
    } else {
        save(settings);
    }
}

#define OPTION_CHOICE(title, options, option)                                                      \
    {title, options, NULL, (int)(sizeof options / sizeof options[0]), NULL, NULL, NULL, option + 1}

ECHO_RESIDENT static const SettingChoice CHOICES[SETTINGS_CHOICE_COUNT] = {
    {STR_SETTINGS_SHUFFLE, SHUFFLE_OPTIONS, NULL, SHUFFLE_MODE_COUNT, current_shuffle,
     choose_shuffle, NULL, 0},
    {STR_SETTINGS_REPEAT, REPEAT_OPTIONS, NULL, REPEAT_MODE_COUNT, current_repeat, choose_repeat,
     NULL, 0},
    {STR_SETTINGS_SLEEP_TIMER, SLEEP_OPTIONS, NULL, SLEEP_TIMER_OPTION_COUNT, current_sleep,
     choose_sleep, NULL, 0},
    {STR_SETTINGS_AUTO_OFF, AUTO_OFF_OPTIONS, NULL, AUTO_OFF_OPTION_COUNT, current_auto_off,
     choose_auto_off, NULL, 0},
    {STR_SETTINGS_SCREEN_TIMEOUT, TIMEOUT_OPTIONS, NULL, SCREEN_TIMEOUT_OPTION_COUNT,
     current_timeout, choose_timeout, NULL, 0},
    {STR_SETTINGS_BRIGHTNESS, NULL, BRIGHTNESS_OPTIONS, BRIGHTNESS_LEVEL_COUNT, current_brightness,
     choose_brightness, NULL, 0},
    {STR_SETTINGS_ORIENTATION, ORIENTATION_OPTIONS, NULL, ORIENTATION_COUNT, current_orientation,
     choose_orientation, NULL, 0},
    {STR_SETTINGS_GAIN, GAIN_OPTIONS, NULL, GAIN_LEVEL_COUNT, current_gain, choose_gain, NULL, 0},
    {STR_SETTINGS_FILTER, NULL, FILTER_OPTIONS, DAC_FILTER_COUNT, current_filter, choose_filter,
     NULL, 0},
    {STR_SETTINGS_BALANCE, NULL, BALANCE_OPTIONS, SETTINGS_BALANCE_STEPS, current_balance,
     choose_balance, NULL, 0},
    {STR_SETTINGS_OUTPUT, OUTPUT_OPTIONS, NULL, OUTPUT_JACK_COUNT, current_output, choose_output,
     describe_output, 0},
    {STR_SETTINGS_START_SCREEN, START_OPTIONS, NULL, START_SCREEN_COUNT, current_start,
     choose_start, NULL, 0},
    {STR_SETTINGS_USB_MODE, USB_MODE_OPTIONS, NULL, USB_MODE_COUNT, current_usb_mode,
     choose_usb_mode, NULL, 0},
    {STR_LANGUAGE_TITLE, LANGUAGE_OPTIONS, NULL, LANGUAGE_COUNT, current_language, choose_language,
     NULL, 0},
    OPTION_CHOICE(STR_SETTINGS_ACCENT, ACCENT_OPTIONS, OPTION_ACCENT),
    OPTION_CHOICE(STR_SETTINGS_HIGHLIGHT, HIGHLIGHT_OPTIONS, OPTION_HIGHLIGHT),
    OPTION_CHOICE(STR_SETTINGS_BATTERY_PERCENT, SHOWN_OPTIONS, OPTION_BATTERY),
    OPTION_CHOICE(STR_SETTINGS_SWAP_VOLUME, SWITCH_OPTIONS, OPTION_VOLUME_KEYS),
    OPTION_CHOICE(STR_SETTINGS_ROCKER, ROCKER_OPTIONS, OPTION_ROCKER),
    OPTION_CHOICE(STR_SETTINGS_MENU_KEY, MENU_KEY_OPTIONS, OPTION_MENU_KEY),
    OPTION_CHOICE(STR_SETTINGS_COVER_ORDER, COVER_ORDER_OPTIONS, OPTION_COVER_ORDER),
    OPTION_CHOICE(STR_SETTINGS_ARTIST_VIEW, ARTIST_VIEW_OPTIONS, OPTION_ARTIST_VIEW),
    OPTION_CHOICE(STR_SETTINGS_LISTEN_HISTORY, SHOWN_OPTIONS, OPTION_LISTEN_HISTORY),
    {STR_SETTINGS_BLUETOOTH_LEVEL, BLUETOOTH_LEVEL_OPTIONS, NULL, 2, current_bluetooth_level,
     choose_bluetooth_level, NULL, 0},
    OPTION_CHOICE(STR_SETTINGS_USB_PROMPT, SHOWN_OPTIONS, OPTION_USB_PROMPT),
    OPTION_CHOICE(STR_MENU_COVER_FLOW, COVER_COUNT_OPTIONS, OPTION_COVER_FLOW_COVERS),
    OPTION_CHOICE(STR_SETTINGS_ANIMATIONS, ANIMATION_OPTIONS, OPTION_ANIMATION_SPEED),
    OPTION_CHOICE(STR_SETTINGS_THEME, THEME_OPTIONS, OPTION_THEME),
    OPTION_CHOICE(STR_SETTINGS_LYRICS, SHOWN_OPTIONS, OPTION_LYRICS),
    OPTION_CHOICE(STR_SETTINGS_FOLDER_VIEW, ARTIST_VIEW_OPTIONS, OPTION_FOLDER_VIEW),
    OPTION_CHOICE(STR_LIBRARY_INTERNAL_STORAGE, SWITCH_OPTIONS, OPTION_INTERNAL_STORAGE),
    OPTION_CHOICE(STR_SETTINGS_CLOCK, SWITCH_OPTIONS, OPTION_CLOCK),
    OPTION_CHOICE(STR_SETTINGS_STATUS_BAR, STATUS_BAR_OPTIONS, OPTION_STATUS_BAR),
};

static ScreenAction open_choice(void *context)
{
    SettingsEntry *entry = context;
    SettingsScreens *screens = entry->owner;
    const SettingChoice *choice = entry->choice;
    bool slot = choice->slot != 0;
    ChoiceConfig config = {
        .title = i18n_text(choice->title),
        .options = choice->plain_options,
        .option_ids = choice->options,
        .option_count = choice->option_count,
        .current = slot ? current_slot : choice->current,
        .choose = slot ? choose_slot : choice->choose,
        .context = slot ? (void *)entry : &screens->context,
        .theme = screens->theme,
    };
    return screen_action_push(choice_screen_init(&screens->choice, config));
}

static const char *choice_value(const void *context)
{
    const SettingsEntry *entry = context;
    int current = entry->choice->slot != 0 ? current_slot(entry)
                                           : entry->choice->current(&entry->owner->context);
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
    screens->items[SETTINGS_DATE_TIME_ITEM] = (MenuItem){.label = i18n_text(STR_SETTINGS_DATE_TIME),
                                                         .select = date_time_screen_open,
                                                         .context = context.date_time,
                                                         .value = date_time_screen_summary,
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

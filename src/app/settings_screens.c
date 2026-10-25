#include "settings_screens.h"

#include "hal/audio.h"
#include "hal/clock.h"

enum {
    BRIGHTNESS_LEVEL_COUNT = SETTINGS_BRIGHTNESS_MAX - SETTINGS_BRIGHTNESS_MIN + 1,
    MILLISECONDS_PER_MINUTE = 60000
};

static const char *const SHUFFLE_OPTIONS[SHUFFLE_MODE_COUNT] = {"Off", "Songs", "Albums"};
static const char *const REPEAT_OPTIONS[REPEAT_MODE_COUNT] = {"Off", "One", "All"};
static const char *const SLEEP_OPTIONS[SLEEP_TIMER_OPTION_COUNT] = {
    "Off", "15 min", "30 min", "45 min", "60 min", "90 min", "120 min"};
static const char *const SLEEP_MESSAGES[SLEEP_TIMER_OPTION_COUNT] = {
    "Sleep timer off",       "Turning off in 15 min", "Turning off in 30 min",
    "Turning off in 45 min", "Turning off in 60 min", "Turning off in 90 min",
    "Turning off in 120 min"};
static const char *const AUTO_OFF_OPTIONS[AUTO_OFF_OPTION_COUNT] = {"Off", "1 min", "5 min",
                                                                    "10 min"};
static const char *const TIMEOUT_OPTIONS[SCREEN_TIMEOUT_OPTION_COUNT] = {
    "10 s", "20 s", "30 s", "1 min", "2 min", "5 min", "Never"};
static const char *const BRIGHTNESS_OPTIONS[BRIGHTNESS_LEVEL_COUNT] = {"1", "2", "3", "4", "5",
                                                                       "6", "7", "8", "9", "10"};
static const char *const GAIN_OPTIONS[GAIN_LEVEL_COUNT] = {"Low", "High"};
static const char *const OUTPUT_OPTIONS[OUTPUT_JACK_COUNT] = {"Auto", "3.5 mm", "4.4 mm"};
static const char *const OUTPUT_MESSAGES[OUTPUT_JACK_COUNT] = {
    "Volume follows the plugged jack", "Using 3.5 mm volume levels", "Using 4.4 mm volume levels"};
static const char *const START_OPTIONS[START_SCREEN_COUNT] = {
    "Main menu", "Now Playing", "Albums", "Artists", "Favorites", "Playlists", "Cover Flow"};
static const char *const USB_MODE_OPTIONS[USB_MODE_COUNT] = {"Storage", "DAC"};
static const char *const USB_MODE_MESSAGES[USB_MODE_COUNT] = {"USB shows the player's drives",
                                                              "USB plays the computer's sound"};

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
        toast_show(context->toast, "Could not save settings", hal_clock_milliseconds());
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
    toast_show(settings->toast, SLEEP_MESSAGES[index], now_ms);
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

static int current_gain(const void *context)
{
    return (int)settings_of(context)->gain;
}

static void choose_gain(void *context, int index)
{
    editable_settings(context)->gain = (GainLevel)index;
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
    toast_show(settings->toast, OUTPUT_MESSAGES[index], hal_clock_milliseconds());
}

static const char *describe_output(int current)
{
    HalAudioJack jack = hal_audio_jack();
    if (current != OUTPUT_AUTO || jack == HAL_AUDIO_JACK_NONE) {
        return OUTPUT_OPTIONS[current];
    }
    return jack == HAL_AUDIO_JACK_BALANCED ? "Auto (4.4 mm)" : "Auto (3.5 mm)";
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
    toast_show(settings->toast, USB_MODE_MESSAGES[index], hal_clock_milliseconds());
}

static const SettingChoice CHOICES[SETTINGS_CHOICE_COUNT] = {
    {"Shuffle", SHUFFLE_OPTIONS, SHUFFLE_MODE_COUNT, current_shuffle, choose_shuffle, NULL},
    {"Repeat", REPEAT_OPTIONS, REPEAT_MODE_COUNT, current_repeat, choose_repeat, NULL},
    {"Sleep timer", SLEEP_OPTIONS, SLEEP_TIMER_OPTION_COUNT, current_sleep, choose_sleep, NULL},
    {"Auto off", AUTO_OFF_OPTIONS, AUTO_OFF_OPTION_COUNT, current_auto_off, choose_auto_off, NULL},
    {"Screen timeout", TIMEOUT_OPTIONS, SCREEN_TIMEOUT_OPTION_COUNT, current_timeout,
     choose_timeout, NULL},
    {"Brightness", BRIGHTNESS_OPTIONS, BRIGHTNESS_LEVEL_COUNT, current_brightness,
     choose_brightness, NULL},
    {"Gain", GAIN_OPTIONS, GAIN_LEVEL_COUNT, current_gain, choose_gain, NULL},
    {"Output", OUTPUT_OPTIONS, OUTPUT_JACK_COUNT, current_output, choose_output, describe_output},
    {"Start screen", START_OPTIONS, START_SCREEN_COUNT, current_start, choose_start, NULL},
    {"USB mode", USB_MODE_OPTIONS, USB_MODE_COUNT, current_usb_mode, choose_usb_mode, NULL},
};

static ScreenAction open_choice(void *context)
{
    SettingsEntry *entry = context;
    SettingsScreens *screens = entry->owner;
    const SettingChoice *choice = entry->choice;
    ChoiceConfig config = {
        .title = choice->title,
        .options = choice->options,
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
    return entry->choice->options[current];
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
    screens->items[SETTINGS_RESCAN_ITEM] = (MenuItem){
        .label = "Rescan library", .select = request_rescan, .context = &screens->context};
    for (int index = 0; index < SETTINGS_CHOICE_COUNT; index++) {
        screens->entries[index] = (SettingsEntry){.choice = &CHOICES[index], .owner = screens};
        int item = index < SETTINGS_RESCAN_ITEM ? index : index + 1;
        screens->items[item] = (MenuItem){.label = CHOICES[index].title,
                                          .select = open_choice,
                                          .context = &screens->entries[index],
                                          .value = choice_value,
                                          .opens = true};
    }
    MenuConfig menu = {.title = "Settings",
                       .items = screens->items,
                       .item_count = SETTINGS_ITEM_COUNT,
                       .theme = theme};
    return menu_screen_init(&screens->menu, menu);
}

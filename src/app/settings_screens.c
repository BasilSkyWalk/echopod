#include "settings_screens.h"

#include "hal/audio.h"
#include "hal/clock.h"

enum {
    BRIGHTNESS_LEVEL_COUNT = SETTINGS_BRIGHTNESS_MAX - SETTINGS_BRIGHTNESS_MIN + 1,
    MILLISECONDS_PER_MINUTE = 60000
};

static const char *const SHUFFLE_OPTIONS[SHUFFLE_MODE_COUNT] = {"关闭", "歌曲", "专辑"};
static const char *const REPEAT_OPTIONS[REPEAT_MODE_COUNT] = {"关闭", "单曲", "全部"};
static const char *const SLEEP_OPTIONS[SLEEP_TIMER_OPTION_COUNT] = {
    "关闭", "15 分钟", "30 分钟", "45 分钟", "60 分钟", "90 分钟", "120 分钟"};
static const char *const SLEEP_MESSAGES[SLEEP_TIMER_OPTION_COUNT] = {
    "睡眠定时已关闭",       "15 分钟后关机", "30 分钟后关机",
    "45 分钟后关机", "60 分钟后关机", "90 分钟后关机",
    "120 分钟后关机"};
static const char *const AUTO_OFF_OPTIONS[AUTO_OFF_OPTION_COUNT] = {"关闭", "1 分钟", "5 分钟",
                                                                    "10 分钟"};
static const char *const TIMEOUT_OPTIONS[SCREEN_TIMEOUT_OPTION_COUNT] = {
    "10 秒", "20 秒", "30 秒", "1 分钟", "2 分钟", "5 分钟", "永不"};
static const char *const BRIGHTNESS_OPTIONS[BRIGHTNESS_LEVEL_COUNT] = {"1", "2", "3", "4", "5",
                                                                       "6", "7", "8", "9", "10"};
static const char *const ORIENTATION_OPTIONS[ORIENTATION_COUNT] = {"横屏", "竖屏",
                                                               "竖屏（翻转）"};
static const char *const GAIN_OPTIONS[GAIN_LEVEL_COUNT] = {"低", "高"};
static const char *const OUTPUT_OPTIONS[OUTPUT_JACK_COUNT] = {"自动", "3.5 mm", "4.4 mm"};
static const char *const OUTPUT_MESSAGES[OUTPUT_JACK_COUNT] = {
    "音量跟随所插的接口", "使用 3.5 mm 音量", "使用 4.4 mm 音量"};
static const char *const START_OPTIONS[START_SCREEN_COUNT] = {
    "主菜单", "正在播放", "专辑", "艺术家", "收藏", "播放列表", "封面浏览"};
static const char *const USB_MODE_OPTIONS[USB_MODE_COUNT] = {"存储", "DAC"};
static const char *const USB_MODE_MESSAGES[USB_MODE_COUNT] = {"USB 显示播放器存储",
                                                              "USB 播放电脑声音"};

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
        toast_show(context->toast, "无法保存设置", hal_clock_milliseconds());
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
    return jack == HAL_AUDIO_JACK_BALANCED ? "自动（4.4 mm）" : "自动（3.5 mm）";
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
    {"随机播放", SHUFFLE_OPTIONS, SHUFFLE_MODE_COUNT, current_shuffle, choose_shuffle, NULL},
    {"循环播放", REPEAT_OPTIONS, REPEAT_MODE_COUNT, current_repeat, choose_repeat, NULL},
    {"睡眠定时", SLEEP_OPTIONS, SLEEP_TIMER_OPTION_COUNT, current_sleep, choose_sleep, NULL},
    {"自动关机", AUTO_OFF_OPTIONS, AUTO_OFF_OPTION_COUNT, current_auto_off, choose_auto_off, NULL},
    {"熄屏时间", TIMEOUT_OPTIONS, SCREEN_TIMEOUT_OPTION_COUNT, current_timeout,
     choose_timeout, NULL},
    {"亮度", BRIGHTNESS_OPTIONS, BRIGHTNESS_LEVEL_COUNT, current_brightness,
     choose_brightness, NULL},
    {"方向", ORIENTATION_OPTIONS, ORIENTATION_COUNT, current_orientation, choose_orientation,
     NULL},
    {"增益", GAIN_OPTIONS, GAIN_LEVEL_COUNT, current_gain, choose_gain, NULL},
    {"输出", OUTPUT_OPTIONS, OUTPUT_JACK_COUNT, current_output, choose_output, describe_output},
    {"启动页面", START_OPTIONS, START_SCREEN_COUNT, current_start, choose_start, NULL},
    {"USB 模式", USB_MODE_OPTIONS, USB_MODE_COUNT, current_usb_mode, choose_usb_mode, NULL},
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
        .label = "重新扫描曲库", .select = request_rescan, .context = &screens->context};
    screens->items[SETTINGS_BLUETOOTH_ITEM] = (MenuItem){.label = "蓝牙",
                                                         .select = bluetooth_screen_open,
                                                         .context = context.bluetooth,
                                                         .value = bluetooth_screen_summary,
                                                         .opens = true};
    screens->items[SETTINGS_EQUALIZER_ITEM] = (MenuItem){.label = "均衡器",
                                                         .select = equalizer_screen_open,
                                                         .context = context.equalizer,
                                                         .value = equalizer_screen_summary,
                                                         .opens = true};
    for (int index = 0; index < SETTINGS_CHOICE_COUNT; index++) {
        screens->entries[index] = (SettingsEntry){.choice = &CHOICES[index], .owner = screens};
        int item = index < SETTINGS_BLUETOOTH_ITEM ? index : index + 1;
        item = item < SETTINGS_EQUALIZER_ITEM ? item : item + 1;
        item = item < SETTINGS_RESCAN_ITEM ? item : item + 1;
        screens->items[item] = (MenuItem){.label = CHOICES[index].title,
                                          .select = open_choice,
                                          .context = &screens->entries[index],
                                          .value = choice_value,
                                          .opens = true};
    }
    MenuConfig menu = {.title = "设置",
                       .items = screens->items,
                       .item_count = SETTINGS_ITEM_COUNT,
                       .theme = theme};
    return menu_screen_init(&screens->menu, menu);
}

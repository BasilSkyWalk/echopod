#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "playback/play_queue.h"

typedef enum GainLevel {
    GAIN_LOW,
    GAIN_HIGH
} GainLevel;

typedef enum OutputJack {
    OUTPUT_AUTO,
    OUTPUT_SINGLE_ENDED,
    OUTPUT_BALANCED
} OutputJack;

typedef enum StartScreen {
    START_MAIN_MENU,
    START_NOW_PLAYING,
    START_ALBUMS,
    START_ARTISTS,
    START_FAVORITES,
    START_PLAYLISTS,
    START_COVER_FLOW,
    START_ALBUM_ARTISTS,
    START_GENRES,
    START_FOLDERS,
    START_ALL_SONGS
} StartScreen;

typedef enum UsbMode {
    USB_MODE_STORAGE,
    USB_MODE_DAC
} UsbMode;

typedef enum Language {
    LANGUAGE_ENGLISH,
    LANGUAGE_CHINESE,
    LANGUAGE_JAPANESE,
    LANGUAGE_KOREAN,
    LANGUAGE_GERMAN,
    LANGUAGE_FRENCH,
    LANGUAGE_SPANISH,
    LANGUAGE_ITALIAN,
    LANGUAGE_PORTUGUESE,
    LANGUAGE_DUTCH,
    LANGUAGE_SWEDISH,
    LANGUAGE_DANISH,
    LANGUAGE_NORWEGIAN,
    LANGUAGE_FINNISH,
    LANGUAGE_RUSSIAN,
    LANGUAGE_UKRAINIAN,
    LANGUAGE_POLISH,
    LANGUAGE_INDONESIAN
} Language;

typedef enum Orientation {
    ORIENTATION_LANDSCAPE,
    ORIENTATION_PORTRAIT,
    ORIENTATION_PORTRAIT_FLIPPED
} Orientation;

typedef enum SettingOption {
    OPTION_BATTERY,
    OPTION_VOLUME_KEYS,
    OPTION_ROCKER,
    OPTION_MENU_KEY,
    OPTION_ACCENT,
    OPTION_HIGHLIGHT,
    OPTION_COVER_ORDER,
    OPTION_ARTIST_VIEW,
    OPTION_COVER_FLOW_COVERS,
    OPTION_ANIMATION_SPEED,
    OPTION_CLOCK,
    OPTION_BLUETOOTH_LEVEL,
    OPTION_USB_PROMPT,
    OPTION_LISTEN_HISTORY,
    OPTION_TIME_ZONE,
    OPTION_THEME,
    OPTION_LYRICS,
    SETTINGS_OPTION_COUNT
} SettingOption;

enum {
    CLOCK_OFF,
    CLOCK_ON
};

enum {
    TIME_ZONE_UNSET = 0,
    TIME_ZONE_OPTION_COUNT = 106
};

enum {
    COVER_FLOW_FIVE_COVERS,
    COVER_FLOW_THREE_COVERS
};

typedef enum AnimationSpeed {
    ANIMATION_NORMAL,
    ANIMATION_SLOWER,
    ANIMATION_FASTER,
    ANIMATION_SPEED_COUNT
} AnimationSpeed;

enum {
    BLUETOOTH_LEVEL_STANDARD,
    BLUETOOTH_LEVEL_LOUDER
};

enum {
    USB_PROMPT_ON,
    USB_PROMPT_OFF
};

enum {
    LISTEN_HISTORY_ON,
    LISTEN_HISTORY_OFF
};

enum {
    THEME_DARK,
    THEME_LIGHT
};

enum {
    LYRICS_ON,
    LYRICS_OFF
};

typedef enum RockerMode {
    ROCKER_VOLUME_SKIP,
    ROCKER_SKIP_VOLUME,
    ROCKER_VOLUME_SEEK,
    ROCKER_MODE_COUNT
} RockerMode;

enum {
    ACCENT_COUNT = 8,
    MENU_KEY_NEXT = 1
};

enum {
    GAIN_LEVEL_COUNT = GAIN_HIGH + 1,
    OUTPUT_JACK_COUNT = OUTPUT_BALANCED + 1,
    START_SCREEN_COUNT = START_ALL_SONGS + 1,
    USB_MODE_COUNT = USB_MODE_DAC + 1,
    ORIENTATION_COUNT = ORIENTATION_PORTRAIT_FLIPPED + 1,
    LANGUAGE_COUNT = LANGUAGE_INDONESIAN + 1,
    SETTINGS_BRIGHTNESS_MIN = 1,
    SETTINGS_BRIGHTNESS_MAX = 10,
    SETTINGS_VOLUME_MAX = 50,
    SETTINGS_DEFAULT_DAC_VOLUME = 30,
    DAC_FILTER_COUNT = 5,
    SETTINGS_BALANCE_STEPS = 25,
    SETTINGS_BALANCE_CENTER = 12,
    SCREEN_TIMEOUT_OPTION_COUNT = 7,
    SLEEP_TIMER_OPTION_COUNT = 7,
    AUTO_OFF_OPTION_COUNT = 4,
    SETTINGS_FILE_SIZE = 48,
    SETTINGS_V8_FILE_SIZE = 32
};

typedef struct Settings {
    uint8_t brightness;
    uint8_t screen_timeout;
    GainLevel gain;
    OutputJack output;
    StartScreen start_screen;
    ShuffleMode shuffle;
    RepeatMode repeat;
    uint8_t volume;
    uint8_t auto_off;
    UsbMode usb_mode;
    uint8_t dac_volume;
    Orientation orientation;
    Language language;
    uint8_t filter;
    uint8_t balance;
    uint8_t options[SETTINGS_OPTION_COUNT];
} Settings;

Settings settings_defaults(void);
uint32_t settings_timeout_option_seconds(int option);
uint32_t settings_sleep_option_minutes(int option);
uint32_t settings_screen_timeout_ms(const Settings *settings);
uint32_t settings_auto_off_option_minutes(int option);
uint32_t settings_auto_off_ms(const Settings *settings);
uint8_t settings_option_count(SettingOption option);
uint32_t settings_animation_ms(const uint8_t *options, uint32_t normal_ms);
void settings_encode(const Settings *settings, uint8_t *bytes);
bool settings_decode(const uint8_t *bytes, uint32_t length, Settings *settings);

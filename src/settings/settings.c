#include "settings/settings.h"

#include "core/hash.h"

enum {
    VERSION = 3,
    FIRST_VERSION = 1,
    AUTO_OFF_VERSION = 2,
    MAGIC_SIZE = 4,
    VERSION_FIELD = 4,
    BRIGHTNESS_FIELD = 6,
    TIMEOUT_FIELD = 7,
    GAIN_FIELD = 8,
    OUTPUT_FIELD = 9,
    START_FIELD = 10,
    SHUFFLE_FIELD = 11,
    REPEAT_FIELD = 12,
    VOLUME_FIELD = 13,
    AUTO_OFF_FIELD = 14,
    USB_MODE_FIELD = 15,
    CHECKSUM_FIELD = 20,
    DEFAULT_BRIGHTNESS = 7,
    DEFAULT_TIMEOUT_OPTION = 2,
    DEFAULT_VOLUME = 12,
    DEFAULT_AUTO_OFF_OPTION = 2,
    MILLISECONDS_PER_MINUTE = 60000,
    MILLISECONDS_PER_SECOND = 1000,
    BITS_PER_BYTE = 8
};

static const uint8_t MAGIC[MAGIC_SIZE] = {'E', 'M', 'S', 'T'};
static const uint16_t TIMEOUT_SECONDS[SCREEN_TIMEOUT_OPTION_COUNT] = {10, 20, 30, 60, 120, 300, 0};
static const uint8_t SLEEP_MINUTES[SLEEP_TIMER_OPTION_COUNT] = {0, 15, 30, 45, 60, 90, 120};
static const uint8_t AUTO_OFF_MINUTES[AUTO_OFF_OPTION_COUNT] = {0, 1, 5, 10};

Settings settings_defaults(void)
{
    return (Settings){
        .brightness = DEFAULT_BRIGHTNESS,
        .screen_timeout = DEFAULT_TIMEOUT_OPTION,
        .gain = GAIN_LOW,
        .output = OUTPUT_AUTO,
        .start_screen = START_MAIN_MENU,
        .shuffle = SHUFFLE_OFF,
        .repeat = REPEAT_OFF,
        .volume = DEFAULT_VOLUME,
        .auto_off = DEFAULT_AUTO_OFF_OPTION,
        .usb_mode = USB_MODE_STORAGE,
    };
}

uint32_t settings_timeout_option_seconds(int option)
{
    return option >= 0 && option < SCREEN_TIMEOUT_OPTION_COUNT ? TIMEOUT_SECONDS[option] : 0;
}

uint32_t settings_sleep_option_minutes(int option)
{
    return option >= 0 && option < SLEEP_TIMER_OPTION_COUNT ? SLEEP_MINUTES[option] : 0;
}

uint32_t settings_screen_timeout_ms(const Settings *settings)
{
    return settings_timeout_option_seconds(settings->screen_timeout) * MILLISECONDS_PER_SECOND;
}

uint32_t settings_auto_off_option_minutes(int option)
{
    return option >= 0 && option < AUTO_OFF_OPTION_COUNT ? AUTO_OFF_MINUTES[option] : 0;
}

uint32_t settings_auto_off_ms(const Settings *settings)
{
    return settings_auto_off_option_minutes(settings->auto_off) * MILLISECONDS_PER_MINUTE;
}

static uint32_t checksum_of(const uint8_t *bytes)
{
    return hash_update(HASH_START, bytes, CHECKSUM_FIELD);
}

static void put_u32(uint8_t *bytes, int offset, uint32_t value)
{
    for (int index = 0; index < 4; index++) {
        bytes[offset + index] = (uint8_t)(value >> (BITS_PER_BYTE * index));
    }
}

static uint32_t get_u32(const uint8_t *bytes, int offset)
{
    uint32_t value = 0;
    for (int index = 3; index >= 0; index--) {
        value = value << BITS_PER_BYTE | bytes[offset + index];
    }
    return value;
}

void settings_encode(const Settings *settings, uint8_t *bytes)
{
    for (int index = 0; index < SETTINGS_FILE_SIZE; index++) {
        bytes[index] = index < MAGIC_SIZE ? MAGIC[index] : 0;
    }
    bytes[VERSION_FIELD] = VERSION;
    bytes[BRIGHTNESS_FIELD] = settings->brightness;
    bytes[TIMEOUT_FIELD] = settings->screen_timeout;
    bytes[GAIN_FIELD] = (uint8_t)settings->gain;
    bytes[OUTPUT_FIELD] = (uint8_t)settings->output;
    bytes[START_FIELD] = (uint8_t)settings->start_screen;
    bytes[SHUFFLE_FIELD] = (uint8_t)settings->shuffle;
    bytes[REPEAT_FIELD] = (uint8_t)settings->repeat;
    bytes[VOLUME_FIELD] = settings->volume;
    bytes[AUTO_OFF_FIELD] = settings->auto_off;
    bytes[USB_MODE_FIELD] = (uint8_t)settings->usb_mode;
    put_u32(bytes, CHECKSUM_FIELD, checksum_of(bytes));
}

static bool header_valid(const uint8_t *bytes)
{
    for (int index = 0; index < MAGIC_SIZE; index++) {
        if (bytes[index] != MAGIC[index]) {
            return false;
        }
    }
    uint8_t version = bytes[VERSION_FIELD];
    return version >= FIRST_VERSION && version <= VERSION && bytes[VERSION_FIELD + 1] == 0 &&
           get_u32(bytes, CHECKSUM_FIELD) == checksum_of(bytes);
}

static bool values_valid(const uint8_t *bytes)
{
    return bytes[BRIGHTNESS_FIELD] >= SETTINGS_BRIGHTNESS_MIN &&
           bytes[BRIGHTNESS_FIELD] <= SETTINGS_BRIGHTNESS_MAX &&
           bytes[TIMEOUT_FIELD] < SCREEN_TIMEOUT_OPTION_COUNT &&
           bytes[GAIN_FIELD] < GAIN_LEVEL_COUNT && bytes[OUTPUT_FIELD] < OUTPUT_JACK_COUNT &&
           bytes[START_FIELD] < START_SCREEN_COUNT && bytes[SHUFFLE_FIELD] < SHUFFLE_MODE_COUNT &&
           bytes[REPEAT_FIELD] < REPEAT_MODE_COUNT && bytes[VOLUME_FIELD] <= SETTINGS_VOLUME_MAX &&
           bytes[AUTO_OFF_FIELD] < AUTO_OFF_OPTION_COUNT && bytes[USB_MODE_FIELD] < USB_MODE_COUNT;
}

bool settings_decode(const uint8_t *bytes, uint32_t length, Settings *settings)
{
    if (length < SETTINGS_FILE_SIZE || !header_valid(bytes) || !values_valid(bytes)) {
        return false;
    }
    *settings = (Settings){
        .brightness = bytes[BRIGHTNESS_FIELD],
        .screen_timeout = bytes[TIMEOUT_FIELD],
        .gain = (GainLevel)bytes[GAIN_FIELD],
        .output = (OutputJack)bytes[OUTPUT_FIELD],
        .start_screen = (StartScreen)bytes[START_FIELD],
        .shuffle = (ShuffleMode)bytes[SHUFFLE_FIELD],
        .repeat = (RepeatMode)bytes[REPEAT_FIELD],
        .volume = bytes[VOLUME_FIELD],
        .auto_off = bytes[VERSION_FIELD] < AUTO_OFF_VERSION ? (uint8_t)DEFAULT_AUTO_OFF_OPTION
                                                            : bytes[AUTO_OFF_FIELD],
        .usb_mode =
            bytes[VERSION_FIELD] < VERSION ? USB_MODE_STORAGE : (UsbMode)bytes[USB_MODE_FIELD],
    };
    return true;
}

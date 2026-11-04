#include "check.h"
#include "core/hash.h"
#include "settings/settings.h"

enum {
    VERSION_FIELD = 4,
    BRIGHTNESS_FIELD = 6,
    AUTO_OFF_FIELD = 14,
    USB_MODE_FIELD = 15,
    DAC_VOLUME_FIELD = 16,
    ORIENTATION_FIELD = 17,
    LANGUAGE_FIELD = 18,
    OLD_CHECKSUM_FIELD = 20,
    VERSION_8_CHECKSUM_FIELD = 28,
    CURRENT_CHECKSUM_FIELD = 32,
    NEVER_OPTION = 6
};

static Settings custom_settings(void)
{
    Settings settings = settings_defaults();
    settings.brightness = 3;
    settings.screen_timeout = 4;
    settings.gain = GAIN_HIGH;
    settings.output = OUTPUT_BALANCED;
    settings.start_screen = START_FAVORITES;
    settings.shuffle = SHUFFLE_ALBUMS;
    settings.repeat = REPEAT_ONE;
    settings.volume = 42;
    settings.auto_off = 3;
    settings.usb_mode = USB_MODE_DAC;
    settings.dac_volume = 37;
    settings.orientation = ORIENTATION_PORTRAIT_FLIPPED;
    settings.language = LANGUAGE_KOREAN;
    return settings;
}

static void rewrite_checksum(uint8_t *bytes)
{
    int field = bytes[VERSION_FIELD] >= 9 ? CURRENT_CHECKSUM_FIELD
                : bytes[VERSION_FIELD] >= 8 ? VERSION_8_CHECKSUM_FIELD
                                            : OLD_CHECKSUM_FIELD;
    uint32_t checksum = hash_update(HASH_START, bytes, field);
    for (int index = 0; index < 4; index++) {
        bytes[field + index] = (uint8_t)(checksum >> (8 * index));
    }
}

static void defaults_are_comfortable(void)
{
    Settings settings = settings_defaults();
    CHECK_EQUAL(7, settings.brightness);
    CHECK_EQUAL(30000, settings_screen_timeout_ms(&settings));
    CHECK_EQUAL(GAIN_LOW, settings.gain);
    CHECK_EQUAL(OUTPUT_AUTO, settings.output);
    CHECK_EQUAL(START_MAIN_MENU, settings.start_screen);
    CHECK_EQUAL(SHUFFLE_OFF, settings.shuffle);
    CHECK_EQUAL(REPEAT_OFF, settings.repeat);
    CHECK_EQUAL(12, settings.volume);
    CHECK_EQUAL(5u * 60000u, settings_auto_off_ms(&settings));
    CHECK_EQUAL(USB_MODE_STORAGE, settings.usb_mode);
    CHECK_EQUAL(30, settings.dac_volume);
    CHECK_EQUAL(ORIENTATION_LANDSCAPE, settings.orientation);
}

static void auto_off_options_are_stocks(void)
{
    CHECK_EQUAL(0, settings_auto_off_option_minutes(0));
    CHECK_EQUAL(1, settings_auto_off_option_minutes(1));
    CHECK_EQUAL(5, settings_auto_off_option_minutes(2));
    CHECK_EQUAL(10, settings_auto_off_option_minutes(AUTO_OFF_OPTION_COUNT - 1));
}

static void a_version_one_file_keeps_its_values_and_gets_the_default_auto_off(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[VERSION_FIELD] = 1;
    bytes[AUTO_OFF_FIELD] = 0;
    rewrite_checksum(bytes);
    Settings decoded;
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(42, decoded.volume);
    CHECK_EQUAL(settings_defaults().auto_off, decoded.auto_off);
    CHECK_EQUAL(USB_MODE_STORAGE, decoded.usb_mode);
}

static void a_version_two_file_keeps_its_auto_off_and_uses_usb_storage(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[VERSION_FIELD] = 2;
    bytes[USB_MODE_FIELD] = 0;
    rewrite_checksum(bytes);
    Settings decoded;
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(3, decoded.auto_off);
    CHECK_EQUAL(USB_MODE_STORAGE, decoded.usb_mode);
}

static void the_usb_mode_and_dac_volume_are_saved_at_bytes_fifteen_and_sixteen(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    CHECK_EQUAL(9, bytes[VERSION_FIELD]);
    CHECK_EQUAL(USB_MODE_DAC, bytes[USB_MODE_FIELD]);
    CHECK_EQUAL(37, bytes[DAC_VOLUME_FIELD]);
}

static void the_orientation_is_saved_at_byte_seventeen(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    CHECK_EQUAL(ORIENTATION_PORTRAIT_FLIPPED, bytes[ORIENTATION_FIELD]);
}

static void a_version_four_file_keeps_its_values_and_loads_in_landscape(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[VERSION_FIELD] = 4;
    bytes[ORIENTATION_FIELD] = 0;
    rewrite_checksum(bytes);
    Settings decoded;
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(ORIENTATION_LANDSCAPE, decoded.orientation);
    CHECK_EQUAL(3, decoded.brightness);
    CHECK_EQUAL(USB_MODE_DAC, decoded.usb_mode);
    CHECK_EQUAL(37, decoded.dac_volume);
}

static void a_version_four_file_ignores_whatever_sits_in_the_orientation_byte(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[VERSION_FIELD] = 4;
    bytes[ORIENTATION_FIELD] = ORIENTATION_PORTRAIT;
    rewrite_checksum(bytes);
    Settings decoded;
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(ORIENTATION_LANDSCAPE, decoded.orientation);
}

static void the_language_is_saved_at_byte_eighteen(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    CHECK_EQUAL(LANGUAGE_KOREAN, bytes[LANGUAGE_FIELD]);
}

static void a_version_five_file_loads_in_english(void)
{
    Settings settings = custom_settings();
    Settings decoded;
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[VERSION_FIELD] = 5;
    bytes[LANGUAGE_FIELD] = LANGUAGE_COUNT;
    rewrite_checksum(bytes);
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(LANGUAGE_ENGLISH, decoded.language);
    CHECK_EQUAL(settings.orientation, decoded.orientation);
}

static void an_unknown_language_is_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[LANGUAGE_FIELD] = LANGUAGE_COUNT;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
}

static void an_unknown_orientation_is_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[ORIENTATION_FIELD] = ORIENTATION_COUNT;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
}

static void a_version_three_file_keeps_its_usb_mode_and_gets_the_default_dac_volume(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[VERSION_FIELD] = 3;
    bytes[DAC_VOLUME_FIELD] = 0;
    rewrite_checksum(bytes);
    Settings decoded;
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(USB_MODE_DAC, decoded.usb_mode);
    CHECK_EQUAL(SETTINGS_DEFAULT_DAC_VOLUME, decoded.dac_volume);
}

static void a_dac_volume_above_the_top_is_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[DAC_VOLUME_FIELD] = SETTINGS_VOLUME_MAX + 1;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
}

static void an_unknown_usb_mode_is_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[USB_MODE_FIELD] = USB_MODE_COUNT;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
}

static void a_newer_version_is_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[VERSION_FIELD] = 10;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
}

static void the_filter_and_balance_share_byte_nineteen(void)
{
    Settings settings = custom_settings();
    settings.filter = 3;
    settings.balance = 20;
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    CHECK_EQUAL(3 << 5 | 20, bytes[19]);
    Settings decoded;
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(3, decoded.filter);
    CHECK_EQUAL(20, decoded.balance);
}

static void a_version_six_file_loads_with_the_first_filter_and_centred_balance(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[VERSION_FIELD] = 6;
    bytes[19] = 0;
    rewrite_checksum(bytes);
    Settings decoded;
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(0, decoded.filter);
    CHECK_EQUAL(SETTINGS_BALANCE_CENTER, decoded.balance);
    CHECK_EQUAL(LANGUAGE_KOREAN, decoded.language);
}

static void an_unknown_filter_or_balance_is_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[19] = DAC_FILTER_COUNT << 5;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
    bytes[19] = SETTINGS_BALANCE_STEPS;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
}

static void timeout_options_end_with_never(void)
{
    CHECK_EQUAL(10, settings_timeout_option_seconds(0));
    CHECK_EQUAL(300, settings_timeout_option_seconds(5));
    CHECK_EQUAL(0, settings_timeout_option_seconds(NEVER_OPTION));
    Settings settings = settings_defaults();
    settings.screen_timeout = NEVER_OPTION;
    CHECK_EQUAL(0, settings_screen_timeout_ms(&settings));
}

static void sleep_options_start_with_off(void)
{
    CHECK_EQUAL(0, settings_sleep_option_minutes(0));
    CHECK_EQUAL(15, settings_sleep_option_minutes(1));
    CHECK_EQUAL(120, settings_sleep_option_minutes(SLEEP_TIMER_OPTION_COUNT - 1));
}

static void settings_survive_a_round_trip(void)
{
    Settings original = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    Settings decoded;
    settings_encode(&original, bytes);
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(original.brightness, decoded.brightness);
    CHECK_EQUAL(original.screen_timeout, decoded.screen_timeout);
    CHECK_EQUAL(original.gain, decoded.gain);
    CHECK_EQUAL(original.output, decoded.output);
    CHECK_EQUAL(original.start_screen, decoded.start_screen);
    CHECK_EQUAL(original.shuffle, decoded.shuffle);
    CHECK_EQUAL(original.repeat, decoded.repeat);
    CHECK_EQUAL(original.volume, decoded.volume);
    CHECK_EQUAL(original.auto_off, decoded.auto_off);
    CHECK_EQUAL(original.usb_mode, decoded.usb_mode);
    CHECK_EQUAL(original.dac_volume, decoded.dac_volume);
    CHECK_EQUAL(original.orientation, decoded.orientation);
    CHECK_EQUAL(original.language, decoded.language);
}

static void a_corrupted_file_is_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[BRIGHTNESS_FIELD] ^= 0x02;
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
    CHECK(!settings_decode(bytes, SETTINGS_FILE_SIZE - 1, &settings));
}

static void out_of_range_values_are_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[BRIGHTNESS_FIELD] = SETTINGS_BRIGHTNESS_MAX + 1;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(defaults_are_comfortable),
        TEST_CASE(timeout_options_end_with_never),
        TEST_CASE(sleep_options_start_with_off),
        TEST_CASE(settings_survive_a_round_trip),
        TEST_CASE(a_corrupted_file_is_rejected),
        TEST_CASE(out_of_range_values_are_rejected),
        TEST_CASE(auto_off_options_are_stocks),
        TEST_CASE(a_version_one_file_keeps_its_values_and_gets_the_default_auto_off),
        TEST_CASE(a_version_two_file_keeps_its_auto_off_and_uses_usb_storage),
        TEST_CASE(the_usb_mode_and_dac_volume_are_saved_at_bytes_fifteen_and_sixteen),
        TEST_CASE(a_version_three_file_keeps_its_usb_mode_and_gets_the_default_dac_volume),
        TEST_CASE(a_dac_volume_above_the_top_is_rejected),
        TEST_CASE(an_unknown_usb_mode_is_rejected),
        TEST_CASE(the_orientation_is_saved_at_byte_seventeen),
        TEST_CASE(a_version_four_file_keeps_its_values_and_loads_in_landscape),
        TEST_CASE(a_version_four_file_ignores_whatever_sits_in_the_orientation_byte),
        TEST_CASE(the_language_is_saved_at_byte_eighteen),
        TEST_CASE(a_version_five_file_loads_in_english),
        TEST_CASE(an_unknown_language_is_rejected),
        TEST_CASE(an_unknown_orientation_is_rejected),
        TEST_CASE(a_newer_version_is_rejected),
        TEST_CASE(the_filter_and_balance_share_byte_nineteen),
        TEST_CASE(a_version_six_file_loads_with_the_first_filter_and_centred_balance),
        TEST_CASE(an_unknown_filter_or_balance_is_rejected),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

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
    CHECKSUM_FIELD = 20,
    OPTIONS_FIELD = 20,
    V8_CHECKSUM_FIELD = 28,
    OPTIONS_CHECKSUM_FIELD = 44,
    V8_OPTION_COUNT = 8,
    SPARE_OPTION_FIELD = 43,
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
    uint32_t checksum = hash_update(HASH_START, bytes, CHECKSUM_FIELD);
    for (int index = 0; index < 4; index++) {
        bytes[CHECKSUM_FIELD + index] = (uint8_t)(checksum >> (8 * index));
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
    bytes[VERSION_FIELD] = 8;
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

static void the_new_options_are_saved_and_read_back(void)
{
    Settings settings = custom_settings();
    for (int option = 0; option < SETTINGS_OPTION_COUNT; option++) {
        settings.options[option] = (uint8_t)(settings_option_count((SettingOption)option) - 1);
    }
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    CHECK_EQUAL(ACCENT_COUNT - 1, bytes[OPTIONS_FIELD + OPTION_ACCENT]);
    Settings decoded;
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    for (int option = 0; option < SETTINGS_OPTION_COUNT; option++) {
        CHECK_EQUAL(settings.options[option], decoded.options[option]);
    }
}

static void an_option_out_of_range_is_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[OPTIONS_FIELD + OPTION_ROCKER] = ROCKER_MODE_COUNT;
    uint32_t checksum = hash_update(HASH_START, bytes, OPTIONS_CHECKSUM_FIELD);
    for (int index = 0; index < 4; index++) {
        bytes[OPTIONS_CHECKSUM_FIELD + index] = (uint8_t)(checksum >> (8 * index));
    }
    Settings decoded;
    CHECK(!settings_decode(bytes, sizeof bytes, &decoded));
}

static void a_version_7_file_loads_with_the_options_off(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[VERSION_FIELD] = 7;
    for (int index = OPTIONS_FIELD; index < SETTINGS_FILE_SIZE; index++) {
        bytes[index] = 0xEE;
    }
    rewrite_checksum(bytes);
    Settings decoded;
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(LANGUAGE_KOREAN, decoded.language);
    for (int option = 0; option < SETTINGS_OPTION_COUNT; option++) {
        CHECK_EQUAL(0, decoded.options[option]);
    }
}

static void put_checksum(uint8_t *bytes, int field)
{
    uint32_t checksum = hash_update(HASH_START, bytes, (size_t)field);
    for (int index = 0; index < 4; index++) {
        bytes[field + index] = (uint8_t)(checksum >> (8 * index));
    }
}

static Settings settings_with_every_option_at_its_top(void)
{
    Settings settings = custom_settings();
    settings.filter = DAC_FILTER_COUNT - 1;
    settings.balance = SETTINGS_BALANCE_STEPS - 1;
    for (int option = 0; option < SETTINGS_OPTION_COUNT; option++) {
        settings.options[option] = (uint8_t)(settings_option_count((SettingOption)option) - 1);
    }
    return settings;
}

static void version_8_bytes(const Settings *settings, uint8_t *bytes)
{
    settings_encode(settings, bytes);
    bytes[VERSION_FIELD] = 8;
    put_checksum(bytes, V8_CHECKSUM_FIELD);
}

static void check_same_values(const Settings *expected, const Settings *actual)
{
    CHECK_EQUAL(expected->brightness, actual->brightness);
    CHECK_EQUAL(expected->screen_timeout, actual->screen_timeout);
    CHECK_EQUAL(expected->gain, actual->gain);
    CHECK_EQUAL(expected->output, actual->output);
    CHECK_EQUAL(expected->start_screen, actual->start_screen);
    CHECK_EQUAL(expected->shuffle, actual->shuffle);
    CHECK_EQUAL(expected->repeat, actual->repeat);
    CHECK_EQUAL(expected->volume, actual->volume);
    CHECK_EQUAL(expected->auto_off, actual->auto_off);
    CHECK_EQUAL(expected->usb_mode, actual->usb_mode);
    CHECK_EQUAL(expected->dac_volume, actual->dac_volume);
    CHECK_EQUAL(expected->orientation, actual->orientation);
    CHECK_EQUAL(expected->language, actual->language);
    CHECK_EQUAL(expected->filter, actual->filter);
    CHECK_EQUAL(expected->balance, actual->balance);
}

static void the_new_options_default_to_their_first_choice(void)
{
    Settings settings = settings_defaults();
    for (int option = OPTION_COVER_FLOW_COVERS; option < SETTINGS_OPTION_COUNT; option++) {
        CHECK_EQUAL(0, settings.options[option]);
    }
    CHECK_EQUAL(COVER_FLOW_FIVE_COVERS, settings.options[OPTION_COVER_FLOW_COVERS]);
    CHECK_EQUAL(ANIMATION_NORMAL, settings.options[OPTION_ANIMATION_SPEED]);
    CHECK_EQUAL(BLUETOOTH_LEVEL_STANDARD, settings.options[OPTION_BLUETOOTH_LEVEL]);
    CHECK_EQUAL(USB_PROMPT_ON, settings.options[OPTION_USB_PROMPT]);
    CHECK_EQUAL(LISTEN_HISTORY_ON, settings.options[OPTION_LISTEN_HISTORY]);
    CHECK_EQUAL(2, settings_option_count(OPTION_COVER_FLOW_COVERS));
    CHECK_EQUAL(3, settings_option_count(OPTION_ANIMATION_SPEED));
    CHECK_EQUAL(2, settings_option_count(OPTION_CLOCK));
    CHECK_EQUAL(2, settings_option_count(OPTION_BLUETOOTH_LEVEL));
    CHECK_EQUAL(2, settings_option_count(OPTION_USB_PROMPT));
    CHECK_EQUAL(2, settings_option_count(OPTION_LISTEN_HISTORY));
}

static void the_animation_speed_scales_animation_times(void)
{
    Settings settings = settings_defaults();
    CHECK_EQUAL(240u, settings_animation_ms(settings.options, 240));
    settings.options[OPTION_ANIMATION_SPEED] = ANIMATION_SLOWER;
    CHECK_EQUAL(360u, settings_animation_ms(settings.options, 240));
    settings.options[OPTION_ANIMATION_SPEED] = ANIMATION_FASTER;
    CHECK_EQUAL(144u, settings_animation_ms(settings.options, 240));
    CHECK_EQUAL(42u, settings_animation_ms(settings.options, 70));
    CHECK_EQUAL(70u, settings_animation_ms(NULL, 70));
}

static void a_version_9_file_is_48_bytes_with_the_checksum_at_the_end(void)
{
    Settings settings = settings_with_every_option_at_its_top();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    CHECK_EQUAL(48, SETTINGS_FILE_SIZE);
    CHECK_EQUAL(9, bytes[VERSION_FIELD]);
    CHECK_EQUAL(ANIMATION_FASTER, bytes[OPTIONS_FIELD + OPTION_ANIMATION_SPEED]);
    CHECK_EQUAL(1, bytes[OPTIONS_FIELD + OPTION_LISTEN_HISTORY]);
    CHECK_EQUAL(0, bytes[SPARE_OPTION_FIELD]);
    uint32_t checksum = hash_update(HASH_START, bytes, OPTIONS_CHECKSUM_FIELD);
    CHECK_EQUAL(checksum, (uint32_t)bytes[44] | (uint32_t)bytes[45] << 8 |
                              (uint32_t)bytes[46] << 16 | (uint32_t)bytes[47] << 24);
    Settings decoded;
    CHECK(!settings_decode(bytes, SETTINGS_FILE_SIZE - 1, &decoded));
    CHECK(!settings_decode(bytes, SETTINGS_V8_FILE_SIZE, &decoded));
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    check_same_values(&settings, &decoded);
    for (int option = 0; option < SETTINGS_OPTION_COUNT; option++) {
        CHECK_EQUAL(settings.options[option], decoded.options[option]);
    }
}

static void a_version_8_file_keeps_every_value_and_gets_the_new_defaults(void)
{
    Settings settings = settings_with_every_option_at_its_top();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    version_8_bytes(&settings, bytes);
    Settings decoded;
    CHECK(settings_decode(bytes, SETTINGS_V8_FILE_SIZE, &decoded));
    check_same_values(&settings, &decoded);
    for (int option = 0; option < V8_OPTION_COUNT; option++) {
        CHECK_EQUAL(settings.options[option], decoded.options[option]);
    }
    for (int option = V8_OPTION_COUNT; option < SETTINGS_OPTION_COUNT; option++) {
        CHECK_EQUAL(0, decoded.options[option]);
    }
    Settings from_a_longer_file;
    CHECK(settings_decode(bytes, sizeof bytes, &from_a_longer_file));
    CHECK_EQUAL(ACCENT_COUNT - 1, from_a_longer_file.options[OPTION_ACCENT]);
    CHECK_EQUAL(0, from_a_longer_file.options[OPTION_LISTEN_HISTORY]);
}

static void a_version_8_file_is_saved_again_as_version_9(void)
{
    Settings settings = settings_with_every_option_at_its_top();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    version_8_bytes(&settings, bytes);
    Settings decoded;
    CHECK(settings_decode(bytes, SETTINGS_V8_FILE_SIZE, &decoded));
    uint8_t again[SETTINGS_FILE_SIZE];
    settings_encode(&decoded, again);
    Settings reloaded;
    CHECK_EQUAL(9, again[VERSION_FIELD]);
    CHECK(settings_decode(again, sizeof again, &reloaded));
    check_same_values(&settings, &reloaded);
    CHECK_EQUAL(ROCKER_MODE_COUNT - 1, reloaded.options[OPTION_ROCKER]);
    CHECK_EQUAL(1, reloaded.options[OPTION_ARTIST_VIEW]);
}

static void a_short_or_corrupted_version_8_file_is_rejected(void)
{
    Settings settings = settings_with_every_option_at_its_top();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    version_8_bytes(&settings, bytes);
    Settings decoded;
    CHECK(!settings_decode(bytes, SETTINGS_V8_FILE_SIZE - 1, &decoded));
    bytes[OPTIONS_FIELD + OPTION_ACCENT] ^= 0x01;
    CHECK(!settings_decode(bytes, SETTINGS_V8_FILE_SIZE, &decoded));
    bytes[OPTIONS_FIELD + OPTION_ACCENT] = ACCENT_COUNT;
    put_checksum(bytes, V8_CHECKSUM_FIELD);
    CHECK(!settings_decode(bytes, SETTINGS_V8_FILE_SIZE, &decoded));
}

static void a_value_in_a_spare_option_slot_is_ignored(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[SPARE_OPTION_FIELD] = 0xEE;
    put_checksum(bytes, OPTIONS_CHECKSUM_FIELD);
    Settings decoded;
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(LANGUAGE_KOREAN, decoded.language);
}

static void a_new_option_out_of_range_is_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[OPTIONS_FIELD + OPTION_ANIMATION_SPEED] = ANIMATION_SPEED_COUNT;
    put_checksum(bytes, OPTIONS_CHECKSUM_FIELD);
    Settings decoded;
    CHECK(!settings_decode(bytes, sizeof bytes, &decoded));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_new_options_are_saved_and_read_back),
        TEST_CASE(the_new_options_default_to_their_first_choice),
        TEST_CASE(the_animation_speed_scales_animation_times),
        TEST_CASE(a_version_9_file_is_48_bytes_with_the_checksum_at_the_end),
        TEST_CASE(a_version_8_file_keeps_every_value_and_gets_the_new_defaults),
        TEST_CASE(a_version_8_file_is_saved_again_as_version_9),
        TEST_CASE(a_short_or_corrupted_version_8_file_is_rejected),
        TEST_CASE(a_value_in_a_spare_option_slot_is_ignored),
        TEST_CASE(a_new_option_out_of_range_is_rejected),
        TEST_CASE(an_option_out_of_range_is_rejected),
        TEST_CASE(a_version_7_file_loads_with_the_options_off),
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

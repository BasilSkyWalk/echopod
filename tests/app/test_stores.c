#include "card_files.h"
#include "check.h"

#include "core/hash.h"
#include "equalizer_store.h"
#include "fake_hal.h"
#include "favorites_store.h"
#include "fixture_library.h"
#include "hal/audio.h"
#include "hal/storage.h"
#include "hal/system.h"
#include "library/cover_build.h"
#include "settings_store.h"
#include <string.h>

enum {
    BRIGHT = 9,
    VERSION_FIELD = 4,
    ORIENTATION_FIELD = 17,
    CHECKSUM_FIELD = 20,
    V8_CHECKSUM_FIELD = 28,
    CHOSEN_ALBUM = 2,
    OTHER_ALBUM = 1
};

static FixtureLibrary fixture;
static SettingsStore settings;
static FavoritesStore favorites;
static EqualizerStore equalizer;

static void without_a_file_the_defaults_are_used(void)
{
    fake_hal_reset();
    settings_store_load(&settings);
    Settings defaults = settings_defaults();
    CHECK_EQUAL(defaults.brightness, settings.settings.brightness);
    CHECK_EQUAL(defaults.volume, settings.settings.volume);
}

static void saved_settings_are_loaded_again(void)
{
    fake_hal_reset();
    settings_store_load(&settings);
    settings.settings.brightness = BRIGHT;
    settings.settings.gain = GAIN_HIGH;
    CHECK(settings_store_save(&settings));
    settings_store_load(&settings);
    CHECK_EQUAL(BRIGHT, settings.settings.brightness);
    CHECK_EQUAL(GAIN_HIGH, settings.settings.gain);
}

static void unchanged_settings_are_not_written_again(void)
{
    fake_hal_reset();
    settings_store_load(&settings);
    CHECK(settings_store_save(&settings));
    int writes = fake_hal_write_count();
    CHECK(settings_store_save(&settings));
    CHECK_EQUAL(writes, fake_hal_write_count());
}

static void a_corrupted_settings_file_falls_back_to_the_defaults(void)
{
    static const uint8_t garbage[SETTINGS_FILE_SIZE] = {'E', 'M', 'S', 'T', 9};
    fake_hal_reset();
    fake_hal_add_file(SETTINGS_STORE_PATH, garbage, sizeof garbage);
    settings_store_load(&settings);
    CHECK_EQUAL(settings_defaults().brightness, settings.settings.brightness);
}

static void a_settings_file_from_before_orientation_loads_in_landscape(void)
{
    Settings older = settings_defaults();
    older.brightness = BRIGHT;
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&older, bytes);
    bytes[VERSION_FIELD] = 4;
    bytes[ORIENTATION_FIELD] = 0;
    uint32_t checksum = hash_update(HASH_START, bytes, CHECKSUM_FIELD);
    for (int index = 0; index < 4; index++) {
        bytes[CHECKSUM_FIELD + index] = (uint8_t)(checksum >> (8 * index));
    }
    fake_hal_reset();
    fake_hal_add_file(SETTINGS_STORE_PATH, bytes, sizeof bytes);
    settings_store_load(&settings);
    CHECK_EQUAL(BRIGHT, settings.settings.brightness);
    CHECK_EQUAL(ORIENTATION_LANDSCAPE, settings.settings.orientation);
}

static void a_32_byte_version_8_settings_file_loads_and_is_saved_as_version_9(void)
{
    Settings older = settings_defaults();
    older.brightness = BRIGHT;
    older.options[OPTION_ACCENT] = 3;
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&older, bytes);
    bytes[VERSION_FIELD] = 8;
    uint32_t checksum = hash_update(HASH_START, bytes, V8_CHECKSUM_FIELD);
    for (int index = 0; index < 4; index++) {
        bytes[V8_CHECKSUM_FIELD + index] = (uint8_t)(checksum >> (8 * index));
    }
    fake_hal_reset();
    fake_hal_add_file(SETTINGS_STORE_PATH, bytes, SETTINGS_V8_FILE_SIZE);
    settings_store_load(&settings);
    CHECK_EQUAL(BRIGHT, settings.settings.brightness);
    CHECK_EQUAL(3, settings.settings.options[OPTION_ACCENT]);
    CHECK_EQUAL(0, settings.settings.options[OPTION_CLOCK]);
    int writes = fake_hal_write_count();
    CHECK(settings_store_save(&settings));
    CHECK_EQUAL(writes + 1, fake_hal_write_count());
    CHECK_EQUAL(9, settings.saved[VERSION_FIELD]);
    uint32_t size = 0;
    fake_hal_file_bytes(SETTINGS_STORE_PATH, &size);
    CHECK_EQUAL(SETTINGS_FILE_SIZE, size);
}

static void the_orientation_is_loaded_again(void)
{
    fake_hal_reset();
    settings_store_load(&settings);
    settings.settings.orientation = ORIENTATION_PORTRAIT_FLIPPED;
    CHECK(settings_store_save(&settings));
    settings_store_load(&settings);
    CHECK_EQUAL(ORIENTATION_PORTRAIT_FLIPPED, settings.settings.orientation);
}

static void hardware_settings_reach_the_hal(void)
{
    fake_hal_reset();
    Settings chosen = settings_defaults();
    chosen.brightness = BRIGHT;
    chosen.gain = GAIN_HIGH;
    chosen.output = OUTPUT_BALANCED;
    chosen.usb_mode = USB_MODE_DAC;
    chosen.options[OPTION_BLUETOOTH_LEVEL] = BLUETOOTH_LEVEL_LOUDER;
    chosen.options[OPTION_USB_PROMPT] = USB_PROMPT_ON;
    chosen.options[OPTION_SPOKEN_SPEED] = SPOKEN_SPEED_2X;
    settings_store_apply_hardware(&chosen);
    CHECK_EQUAL(BRIGHT, fake_hal_display_brightness());
    CHECK_EQUAL(HAL_AUDIO_GAIN_HIGH, fake_hal_audio_gain());
    CHECK_EQUAL(HAL_AUDIO_OUTPUT_BALANCED, fake_hal_audio_output());
    CHECK_EQUAL(HAL_AUDIO_BLUETOOTH_LOUDER, fake_hal_audio_bluetooth_level());
    CHECK_EQUAL(HAL_USB_MODE_DAC, fake_hal_usb_mode());
    CHECK(fake_hal_usb_prompt());
    CHECK_EQUAL(SETTINGS_DEFAULT_DAC_VOLUME, fake_hal_dac_volume());
    CHECK_EQUAL(0, fake_hal_audio_speed_calls());
}

static void the_spoken_speed_setting_becomes_a_hal_speed(void)
{
    Settings chosen = settings_defaults();
    HalAudioSpeed normal = settings_store_spoken_speed(&chosen);
    CHECK_EQUAL(4, normal.quarters);
    CHECK(normal.keep_pitch);
    chosen.options[OPTION_SPOKEN_SPEED] = SPOKEN_SPEED_1_5X;
    chosen.options[OPTION_SPOKEN_PITCH] = SPOKEN_PITCH_FOLLOWS_SPEED;
    HalAudioSpeed fast = settings_store_spoken_speed(&chosen);
    CHECK_EQUAL(6, fast.quarters);
    CHECK(!fast.keep_pitch);
}

static void the_saved_spoken_speed_reaches_the_player_at_start(void)
{
    fake_hal_reset();
    Player player;
    player_init(&player, fixture_library_open(&fixture));
    settings.settings = settings_defaults();
    settings.settings.options[OPTION_SPOKEN_SPEED] = SPOKEN_SPEED_2X;
    settings.settings.options[OPTION_SPOKEN_PITCH] = SPOKEN_PITCH_FOLLOWS_SPEED;
    settings_store_restore_playback(&settings, &player, 1);
    CHECK_EQUAL(8, player.spoken_speed.quarters);
    CHECK(!player.spoken_speed.keep_pitch);
    CHECK_EQUAL(0, fake_hal_audio_speed_calls());
}

static void toggling_an_album_adds_it_and_saves(void)
{
    fake_hal_reset();
    favorites_store_load(&favorites, fixture_library_open(&fixture));
    FavoriteResult result = favorites_store_toggle_album(&favorites, CHOSEN_ALBUM);
    CHECK_EQUAL(FAVORITE_ADDED, result.change);
    CHECK(result.saved);
    CHECK(favorites_store_contains_album(&favorites, CHOSEN_ALBUM));
    CHECK(!favorites_store_contains_album(&favorites, OTHER_ALBUM));
}

static void favorites_are_loaded_again(void)
{
    fake_hal_reset();
    Library *library = fixture_library_open(&fixture);
    favorites_store_load(&favorites, library);
    favorites_store_toggle_album(&favorites, CHOSEN_ALBUM);
    favorites_store_load(&favorites, library);
    CHECK(favorites_store_contains_album(&favorites, CHOSEN_ALBUM));
    CHECK_EQUAL(FAVORITE_REMOVED, favorites_store_toggle_album(&favorites, CHOSEN_ALBUM).change);
    favorites_store_load(&favorites, library);
    CHECK(!favorites_store_contains_album(&favorites, CHOSEN_ALBUM));
}

static void an_empty_favorites_list_answers_without_reading_the_library(void)
{
    fake_hal_reset();
    FavoritesStore blank = {0};
    favorites_store_load(&blank, fixture_library_open(&fixture));
    fixture.memory.reads = 0;
    CHECK(!favorites_store_contains_album(&blank, CHOSEN_ALBUM));
    CHECK_EQUAL(0, fixture.memory.reads);
}

static const char COVERS_PATH[] = "/.echo/covers.bin";

enum {
    COVER_SLOT = 8 + 4 + COVER_BUILD_RECORD_BYTES,
    DEFAULT_COVERS_BYTES = 80 * 1024 * 1024,
    DEFAULT_COVER_SLOTS = 2673
};

static uint32_t covers_size(void)
{
    HalFile file;
    CHECK(hal_file_open(&file, COVERS_PATH));
    uint32_t size = file.size;
    hal_file_close(&file);
    return size;
}

static void the_covers_file_is_sized_with_room_to_spare(void)
{
    CHECK_EQUAL(16 + 3840u * COVER_SLOT, card_files_covers_size(3000));
    CHECK_EQUAL(16 + 3584u * COVER_SLOT, card_files_covers_size(DEFAULT_COVER_SLOTS));
    CHECK_EQUAL(16 + 8192u * COVER_SLOT, card_files_covers_size(20000));
}

static void a_covers_file_that_fits_the_albums_is_left_alone(void)
{
    fake_hal_reset();
    fake_hal_add_file(COVERS_PATH, NULL, DEFAULT_COVERS_BYTES);
    card_files_fit_covers(DEFAULT_COVER_SLOTS);
    CHECK_EQUAL(0, fake_hal_grow_count());
    CHECK_EQUAL(DEFAULT_COVERS_BYTES, covers_size());
}

static void a_covers_file_grows_for_more_albums(void)
{
    fake_hal_reset();
    fake_hal_add_file(COVERS_PATH, NULL, DEFAULT_COVERS_BYTES);
    card_files_fit_covers(DEFAULT_COVER_SLOTS + 1);
    CHECK_EQUAL(card_files_covers_size(DEFAULT_COVER_SLOTS + 1), covers_size());
    card_files_fit_covers(20000);
    CHECK_EQUAL(16 + 8192u * COVER_SLOT, covers_size());
}

static void a_card_without_room_to_spare_gets_room_for_just_the_albums(void)
{
    fake_hal_reset();
    fake_hal_add_file(COVERS_PATH, NULL, DEFAULT_COVERS_BYTES);
    fake_hal_set_grow_limit(16 + 3000u * COVER_SLOT);
    card_files_fit_covers(3000);
    CHECK_EQUAL(16 + 3000u * COVER_SLOT, covers_size());
    CHECK_EQUAL(2, fake_hal_grow_count());
}

static void without_a_covers_file_nothing_grows(void)
{
    fake_hal_reset();
    card_files_fit_covers(3000);
    CHECK_EQUAL(0, fake_hal_grow_count());
}

static void a_file_preset_is_loaded_and_applied(void)
{
    static const char TEXT[] = "Preamp: -1 dB\n"
                               "Filter 1: ON PK Fc 1000 Hz Gain 2 dB Q 1.5\n";
    EqualizerChoice choice = {.preset = EQUALIZER_PRESET_FILE, .preset_name = "test.txt"};
    uint8_t bytes[EQUALIZER_FILE_SIZE];
    equalizer_file_encode(&choice, bytes);
    fake_hal_reset();
    fake_hal_add_file(EQUALIZER_STORE_PATH, bytes, sizeof bytes);
    fake_hal_add_file("/EQ/test.txt", (const uint8_t *)TEXT, sizeof TEXT - 1);
    equalizer_store_load(&equalizer);
    CHECK(equalizer_store_apply(&equalizer));
    CHECK_EQUAL(EQUALIZER_PRESET_FILE, equalizer.choice.preset);
    CHECK_EQUAL(1, fake_hal_equalizer().band_count);
    CHECK_EQUAL(20, fake_hal_equalizer().bands[0].gain_tenths);
    CHECK_EQUAL(-10, fake_hal_equalizer().preamp_tenths);
}

static void a_missing_file_preset_falls_back_to_off(void)
{
    EqualizerChoice choice = {.preset = EQUALIZER_PRESET_FILE, .preset_name = "missing.txt"};
    uint8_t bytes[EQUALIZER_FILE_SIZE];
    equalizer_file_encode(&choice, bytes);
    fake_hal_reset();
    fake_hal_add_file(EQUALIZER_STORE_PATH, bytes, sizeof bytes);
    equalizer_store_load(&equalizer);
    CHECK(!equalizer_store_apply(&equalizer));
    CHECK_EQUAL(EQUALIZER_PRESET_OFF, equalizer.choice.preset);
    CHECK_EQUAL('\0', equalizer.choice.preset_name[0]);
    CHECK_EQUAL(0, fake_hal_equalizer().band_count);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(without_a_file_the_defaults_are_used),
        TEST_CASE(saved_settings_are_loaded_again),
        TEST_CASE(unchanged_settings_are_not_written_again),
        TEST_CASE(a_corrupted_settings_file_falls_back_to_the_defaults),
        TEST_CASE(a_settings_file_from_before_orientation_loads_in_landscape),
        TEST_CASE(a_32_byte_version_8_settings_file_loads_and_is_saved_as_version_9),
        TEST_CASE(the_orientation_is_loaded_again),
        TEST_CASE(hardware_settings_reach_the_hal),
        TEST_CASE(the_spoken_speed_setting_becomes_a_hal_speed),
        TEST_CASE(the_saved_spoken_speed_reaches_the_player_at_start),
        TEST_CASE(toggling_an_album_adds_it_and_saves),
        TEST_CASE(favorites_are_loaded_again),
        TEST_CASE(an_empty_favorites_list_answers_without_reading_the_library),
        TEST_CASE(the_covers_file_is_sized_with_room_to_spare),
        TEST_CASE(a_covers_file_that_fits_the_albums_is_left_alone),
        TEST_CASE(a_covers_file_grows_for_more_albums),
        TEST_CASE(a_card_without_room_to_spare_gets_room_for_just_the_albums),
        TEST_CASE(without_a_covers_file_nothing_grows),
        TEST_CASE(a_file_preset_is_loaded_and_applied),
        TEST_CASE(a_missing_file_preset_falls_back_to_off),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

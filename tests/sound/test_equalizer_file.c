#include "check.h"
#include "core/hash.h"
#include "sound/equalizer_file.h"

enum {
    PRESET_FIELD = 6,
    GAINS_FIELD = 7,
    CHECKSUM_FIELD = 16
};

static EqualizerChoice custom_choice(void)
{
    EqualizerChoice choice = {.preset = EQUALIZER_PRESET_CUSTOM,
                              .custom_gains = {-12, -3, 0, 2, 5, 9, 12}};
    return choice;
}

static void rewrite_checksum(uint8_t *bytes)
{
    uint32_t checksum = hash_update(HASH_START, bytes, CHECKSUM_FIELD);
    for (int index = 0; index < 4; index++) {
        bytes[CHECKSUM_FIELD + index] = (uint8_t)(checksum >> (8 * index));
    }
}

static void the_default_is_off_and_flat(void)
{
    EqualizerChoice choice = equalizer_file_defaults();
    CHECK_EQUAL(EQUALIZER_PRESET_OFF, choice.preset);
    for (int band = 0; band < EQUALIZER_CUSTOM_BANDS; band++) {
        CHECK_EQUAL(0, choice.custom_gains[band]);
    }
    CHECK_EQUAL(0, equalizer_file_curve(&choice).band_count);
}

static void a_choice_survives_a_round_trip(void)
{
    EqualizerChoice saved = custom_choice();
    uint8_t bytes[EQUALIZER_FILE_SIZE];
    equalizer_file_encode(&saved, bytes);
    EqualizerChoice loaded = equalizer_file_defaults();
    CHECK(equalizer_file_decode(bytes, sizeof bytes, &loaded));
    CHECK_EQUAL(EQUALIZER_PRESET_CUSTOM, loaded.preset);
    for (int band = 0; band < EQUALIZER_CUSTOM_BANDS; band++) {
        CHECK_EQUAL(saved.custom_gains[band], loaded.custom_gains[band]);
    }
}

static void a_corrupted_or_short_file_is_rejected(void)
{
    EqualizerChoice choice = custom_choice();
    uint8_t bytes[EQUALIZER_FILE_SIZE];
    equalizer_file_encode(&choice, bytes);
    CHECK(!equalizer_file_decode(bytes, EQUALIZER_FILE_SIZE - 1, &choice));
    bytes[GAINS_FIELD] ^= 1;
    CHECK(!equalizer_file_decode(bytes, sizeof bytes, &choice));
    uint8_t zeros[EQUALIZER_FILE_SIZE] = {0};
    CHECK(!equalizer_file_decode(zeros, sizeof zeros, &choice));
}

static void out_of_range_values_are_rejected(void)
{
    EqualizerChoice choice = custom_choice();
    uint8_t bytes[EQUALIZER_FILE_SIZE];
    equalizer_file_encode(&choice, bytes);
    bytes[PRESET_FIELD] = EQUALIZER_PRESET_COUNT;
    rewrite_checksum(bytes);
    CHECK(!equalizer_file_decode(bytes, sizeof bytes, &choice));
    equalizer_file_encode(&choice, bytes);
    bytes[GAINS_FIELD + 3] = 25;
    rewrite_checksum(bytes);
    CHECK(!equalizer_file_decode(bytes, sizeof bytes, &choice));
}

static void the_custom_curve_uses_seven_bands_from_63_hz_to_16_khz(void)
{
    EqualizerChoice choice = custom_choice();
    EqualizerCurve curve = equalizer_file_curve(&choice);
    CHECK_EQUAL(EQUALIZER_CUSTOM_BANDS, curve.band_count);
    CHECK_EQUAL(63, curve.bands[0].frequency_hz);
    CHECK_EQUAL(1000, curve.bands[3].frequency_hz);
    CHECK_EQUAL(16000, curve.bands[6].frequency_hz);
    CHECK_EQUAL(-12, curve.bands[0].gain_db);
    CHECK_EQUAL(12, curve.bands[6].gain_db);
}

static void the_stock_presets_keep_their_bands(void)
{
    EqualizerChoice choice = {.preset = EQUALIZER_PRESET_BASS};
    EqualizerCurve bass = equalizer_file_curve(&choice);
    CHECK_EQUAL(2, bass.band_count);
    CHECK_EQUAL(63, bass.bands[0].frequency_hz);
    CHECK_EQUAL(5, bass.bands[0].gain_db);
    CHECK_EQUAL(125, bass.bands[1].frequency_hz);
    CHECK_EQUAL(3, bass.bands[1].gain_db);
    choice.preset = EQUALIZER_PRESET_RETRO;
    EqualizerCurve retro = equalizer_file_curve(&choice);
    CHECK_EQUAL(4, retro.band_count);
    CHECK_EQUAL(10000, retro.bands[3].frequency_hz);
    CHECK_EQUAL(-6, retro.bands[3].gain_db);
    choice.preset = EQUALIZER_PRESET_JAZZ;
    CHECK_EQUAL(5, equalizer_file_curve(&choice).band_count);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_default_is_off_and_flat),
        TEST_CASE(a_choice_survives_a_round_trip),
        TEST_CASE(a_corrupted_or_short_file_is_rejected),
        TEST_CASE(out_of_range_values_are_rejected),
        TEST_CASE(the_custom_curve_uses_seven_bands_from_63_hz_to_16_khz),
        TEST_CASE(the_stock_presets_keep_their_bands),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

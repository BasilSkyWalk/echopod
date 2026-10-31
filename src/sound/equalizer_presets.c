#include "sound/equalizer_presets.h"

enum {
    PRESET_MAX_BANDS = 5,
    PRESET_Q_TENTHS = 14,
    CUSTOM_Q_TENTHS = 10
};

typedef struct PresetBand {
    uint16_t frequency_hz;
    int8_t gain_db;
} PresetBand;

typedef struct PresetShape {
    uint8_t band_count;
    PresetBand bands[PRESET_MAX_BANDS];
} PresetShape;

const uint16_t EQUALIZER_CUSTOM_FREQUENCIES[EQUALIZER_CUSTOM_BANDS] = {63,   160,  400,  1000,
                                                                       2500, 6300, 16000};

static const PresetShape SHAPES[EQUALIZER_PRESET_CUSTOM] = {
    [EQUALIZER_PRESET_OFF] = {0, {{0, 0}}},
    [EQUALIZER_PRESET_RETRO] = {4, {{100, 1}, {315, 1}, {1000, 1}, {10000, -6}}},
    [EQUALIZER_PRESET_BASS] = {2, {{63, 5}, {125, 3}}},
    [EQUALIZER_PRESET_HEAVY] = {3, {{315, 3}, {1000, 3}, {3150, -3}}},
    [EQUALIZER_PRESET_POP] = {3, {{100, 4}, {3150, -3}, {10000, 3}}},
    [EQUALIZER_PRESET_JAZZ] = {5, {{100, 4}, {315, -6}, {1000, 4}, {3150, 1}, {10000, 6}}},
    [EQUALIZER_PRESET_UNIQUE] = {3, {{100, 3}, {1000, -3}, {3150, 3}}},
};

static EqualizerCurve custom_curve(const int8_t *gains)
{
    EqualizerCurve curve = {.band_count = EQUALIZER_CUSTOM_BANDS};
    for (int index = 0; index < EQUALIZER_CUSTOM_BANDS; index++) {
        curve.bands[index] = (EqualizerBand){.frequency_hz = EQUALIZER_CUSTOM_FREQUENCIES[index],
                                             .gain_db = gains[index],
                                             .q_tenths = CUSTOM_Q_TENTHS};
    }
    return curve;
}

EqualizerCurve equalizer_presets_curve(EqualizerPreset preset, const int8_t *custom_gains)
{
    if (preset == EQUALIZER_PRESET_CUSTOM) {
        return custom_curve(custom_gains);
    }
    EqualizerCurve curve = {0};
    if ((unsigned)preset >= EQUALIZER_PRESET_CUSTOM) {
        return curve;
    }
    const PresetShape *shape = &SHAPES[preset];
    curve.band_count = shape->band_count;
    for (int index = 0; index < shape->band_count; index++) {
        curve.bands[index] = (EqualizerBand){.frequency_hz = shape->bands[index].frequency_hz,
                                             .gain_db = shape->bands[index].gain_db,
                                             .q_tenths = PRESET_Q_TENTHS};
    }
    return curve;
}

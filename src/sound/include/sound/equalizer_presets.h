#pragma once

#include <stdint.h>

#include "dsp/equalizer.h"

typedef enum EqualizerPreset {
    EQUALIZER_PRESET_OFF,
    EQUALIZER_PRESET_RETRO,
    EQUALIZER_PRESET_BASS,
    EQUALIZER_PRESET_HEAVY,
    EQUALIZER_PRESET_POP,
    EQUALIZER_PRESET_JAZZ,
    EQUALIZER_PRESET_UNIQUE,
    EQUALIZER_PRESET_CUSTOM
} EqualizerPreset;

enum {
    EQUALIZER_PRESET_COUNT = EQUALIZER_PRESET_CUSTOM + 1,
    EQUALIZER_CUSTOM_BANDS = 7
};

extern const uint16_t EQUALIZER_CUSTOM_FREQUENCIES[EQUALIZER_CUSTOM_BANDS];

EqualizerCurve equalizer_presets_curve(EqualizerPreset preset, const int8_t *custom_gains);

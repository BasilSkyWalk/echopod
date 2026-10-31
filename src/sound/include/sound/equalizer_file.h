#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sound/equalizer_presets.h"

enum {
    EQUALIZER_FILE_SIZE = 20
};

typedef struct EqualizerChoice {
    EqualizerPreset preset;
    int8_t custom_gains[EQUALIZER_CUSTOM_BANDS];
} EqualizerChoice;

EqualizerChoice equalizer_file_defaults(void);
void equalizer_file_encode(const EqualizerChoice *choice, uint8_t *bytes);
bool equalizer_file_decode(const uint8_t *bytes, uint32_t length, EqualizerChoice *choice);
EqualizerCurve equalizer_file_curve(const EqualizerChoice *choice);

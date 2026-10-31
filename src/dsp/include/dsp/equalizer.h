#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    EQUALIZER_MAX_BANDS = 8,
    EQUALIZER_GAIN_MIN_DB = -12,
    EQUALIZER_GAIN_MAX_DB = 12,
    EQUALIZER_MAX_RATE = 96000,
    EQUALIZER_HISTORY = 4
};

typedef struct EqualizerBand {
    uint16_t frequency_hz;
    int8_t gain_db;
    uint8_t q_tenths;
} EqualizerBand;

typedef struct EqualizerCurve {
    uint8_t band_count;
    EqualizerBand bands[EQUALIZER_MAX_BANDS];
} EqualizerCurve;

typedef struct EqualizerStage {
    int32_t g;
    int32_t a1;
    int32_t m1;
    uint8_t band;
} EqualizerStage;

typedef struct EqualizerBlock {
    int32_t *left;
    int32_t *right;
    uint32_t count;
    uint32_t stride;
} EqualizerBlock;

typedef struct Equalizer {
    EqualizerCurve curve;
    uint32_t rate;
    int32_t preamp;
    int stage_count;
    EqualizerStage stages[EQUALIZER_MAX_BANDS];
    int32_t left[EQUALIZER_MAX_BANDS][EQUALIZER_HISTORY];
    int32_t right[EQUALIZER_MAX_BANDS][EQUALIZER_HISTORY];
} Equalizer;

void equalizer_init(Equalizer *equalizer);
void equalizer_set_curve(Equalizer *equalizer, const EqualizerCurve *curve);
void equalizer_set_rate(Equalizer *equalizer, uint32_t rate);
bool equalizer_active(const Equalizer *equalizer);
void equalizer_process(Equalizer *equalizer, int32_t *frames, uint32_t count);
void equalizer_process_block(Equalizer *equalizer, EqualizerBlock block);
int equalizer_preamp_db(const EqualizerCurve *curve, uint32_t rate);
bool equalizer_band_audible(EqualizerBand band, uint32_t rate);

#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    EQUALIZER_MAX_BANDS = 10,
    EQUALIZER_GAIN_MIN_DB = -12,
    EQUALIZER_GAIN_MAX_DB = 12,
    EQUALIZER_GAIN_MIN_TENTHS = -120,
    EQUALIZER_GAIN_MAX_TENTHS = 120,
    EQUALIZER_Q_MIN_HUNDREDTHS = 10,
    EQUALIZER_Q_MAX_HUNDREDTHS = 1000,
    EQUALIZER_PREAMP_MIN_TENTHS = -240,
    EQUALIZER_PREAMP_MAX_TENTHS = 120,
    EQUALIZER_MAX_RATE = 96000,
    EQUALIZER_HISTORY = 4,
    EQUALIZER_FADE_FRAMES = 256,
    EQUALIZER_FADE_CHUNK = 32
};

typedef enum EqualizerFilter {
    EQUALIZER_PEAK,
    EQUALIZER_LOW_SHELF,
    EQUALIZER_HIGH_SHELF
} EqualizerFilter;

typedef struct EqualizerBand {
    uint16_t frequency_hz;
    int16_t gain_tenths;
    uint16_t q_hundredths;
    uint8_t filter;
} EqualizerBand;

typedef struct EqualizerCurve {
    uint8_t band_count;
    uint8_t preamp_set;
    int16_t preamp_tenths;
    EqualizerBand bands[EQUALIZER_MAX_BANDS];
} EqualizerCurve;

typedef struct EqualizerStage {
    int32_t g;
    int32_t a1;
    int32_t m1;
    int32_t m2;
    uint8_t band;
} EqualizerStage;

typedef struct EqualizerChain {
    EqualizerCurve curve;
    int32_t preamp;
    int stage_count;
    uint8_t valid;
    EqualizerStage stages[EQUALIZER_MAX_BANDS];
    int32_t left[EQUALIZER_MAX_BANDS][EQUALIZER_HISTORY];
    int32_t right[EQUALIZER_MAX_BANDS][EQUALIZER_HISTORY];
} EqualizerChain;

typedef struct EqualizerBlock {
    int32_t *left;
    int32_t *right;
    uint32_t count;
    uint32_t stride;
} EqualizerBlock;

typedef struct Equalizer {
    uint32_t rate;
    EqualizerChain current;
    EqualizerCurve pending_curve;
    uint8_t has_pending;
    uint8_t fading_out;
    uint16_t fade_remaining;
} Equalizer;

void equalizer_init(Equalizer *equalizer);
void equalizer_reset(Equalizer *equalizer);
void equalizer_set_curve(Equalizer *equalizer, const EqualizerCurve *curve);
void equalizer_set_rate(Equalizer *equalizer, uint32_t rate);
void equalizer_apply_pending(Equalizer *equalizer);
bool equalizer_active(const Equalizer *equalizer);
bool equalizer_valid(const Equalizer *equalizer);
void equalizer_process(Equalizer *equalizer, int32_t *frames, uint32_t count);
void equalizer_process_block(Equalizer *equalizer, EqualizerBlock block);
int equalizer_preamp_tenths(const EqualizerCurve *curve, uint32_t rate);
bool equalizer_curve_valid(const EqualizerCurve *curve, uint32_t rate);
bool equalizer_band_audible(EqualizerBand band, uint32_t rate);

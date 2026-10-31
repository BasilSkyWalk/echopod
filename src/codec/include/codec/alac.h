#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "codec/flac.h"

enum {
    ALAC_CONFIG_BYTES = 24,
    ALAC_MAX_COEFFICIENTS = 32,
    ALAC_READER_BYTES = 4096,
    ALAC_READER_PADDING = 8
};

typedef enum AlacResult {
    ALAC_OK,
    ALAC_ERROR,
    ALAC_UNSUPPORTED
} AlacResult;

typedef struct AlacConfig {
    uint32_t frame_length;
    uint32_t sample_rate;
    uint16_t max_run;
    uint8_t bit_depth;
    uint8_t pb;
    uint8_t mb;
    uint8_t kb;
    uint8_t channels;
} AlacConfig;

typedef struct AlacReader {
    FlacInput input;
    uint32_t base;
    uint32_t length;
    uint32_t bit;
    uint32_t end;
    bool failed;
    uint8_t buffer[ALAC_READER_BYTES + ALAC_READER_PADDING];
} AlacReader;

typedef struct AlacDecoder {
    AlacReader reader;
    AlacConfig config;
    uint32_t block_samples;
    int32_t samples[FLAC_MAX_CHANNELS][FLAC_MAX_BLOCK];
} AlacDecoder;

bool alac_parse_config(const uint8_t *bytes, uint32_t length, AlacConfig *config);
void alac_start(AlacDecoder *decoder, AlacConfig config);
AlacResult alac_decode_frame(AlacDecoder *decoder, FlacInput input, uint32_t offset,
                             uint32_t length);
const int32_t *alac_channel(const AlacDecoder *decoder, int channel);

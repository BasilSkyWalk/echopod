#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    FLAC_MAX_CHANNELS = 2,
    FLAC_MAX_BLOCK = 4608,
    FLAC_MAX_BITS = 24,
    FLAC_BUFFER_BYTES = 4096
};

typedef enum FlacResult {
    FLAC_OK,
    FLAC_END,
    FLAC_ERROR,
    FLAC_UNSUPPORTED
} FlacResult;

typedef struct FlacInput {
    bool (*read)(void *context, uint32_t offset, void *buffer, uint32_t length);
    void *context;
    uint32_t size;
} FlacInput;

typedef struct FlacInfo {
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint16_t min_block;
    uint16_t max_block;
    uint32_t total_samples;
} FlacInfo;

typedef struct FlacBits {
    FlacInput input;
    uint8_t buffer[FLAC_BUFFER_BYTES];
    uint32_t buffer_offset;
    uint32_t buffer_length;
    uint32_t index;
    uint32_t crc_index;
    uint32_t word;
    uint32_t bits;
    uint16_t crc16;
    uint8_t crc8;
    bool failed;
} FlacBits;

typedef struct FlacDecoder {
    FlacBits bits;
    FlacInfo info;
    uint32_t audio_offset;
    uint32_t decoded_samples;
    uint32_t block_samples;
    uint32_t frame_first_sample;
    int32_t samples[FLAC_MAX_CHANNELS][FLAC_MAX_BLOCK];
} FlacDecoder;

FlacResult flac_open(FlacDecoder *decoder, FlacInput input);
FlacResult flac_read_header(FlacBits *bits, FlacInput input, FlacInfo *info,
                            uint32_t *audio_offset);
void flac_start(FlacDecoder *decoder, FlacInput input, FlacInfo info);
uint32_t flac_position(const FlacDecoder *decoder);
void flac_rewind(FlacDecoder *decoder, uint32_t position);
FlacResult flac_decode_frame(FlacDecoder *decoder);
FlacResult flac_seek(FlacDecoder *decoder, uint32_t sample);
const int32_t *flac_channel(const FlacDecoder *decoder, int channel);

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "codec/flac.h"

enum {
    QOA_FILE_HEADER_BYTES = 8,
    QOA_SNIFF_BYTES = 16,
    QOA_BITS_PER_SAMPLE = 16,
    QOA_MAX_PLAYBACK_CHANNELS = 2,
    QOA_SLICE_SAMPLES = 20,
    QOA_FRAME_SLICES = 256,
    QOA_FRAME_SAMPLES = QOA_FRAME_SLICES * QOA_SLICE_SAMPLES,
    QOA_MAX_FRAME_BYTES = 8 + 16 * QOA_MAX_PLAYBACK_CHANNELS +
        8 * QOA_FRAME_SLICES * QOA_MAX_PLAYBACK_CHANNELS,
    QOA_BLOCK_SAMPLES = FLAC_MAX_BLOCK
};

typedef enum QoaResult {
    QOA_OK,
    QOA_END,
    QOA_ERROR,
    QOA_UNSUPPORTED
} QoaResult;

typedef struct QoaInfo {
    uint32_t sample_rate;
    uint8_t channels;
    uint32_t total_samples;
    uint16_t frame_samples;
    uint16_t frame_bytes;
} QoaInfo;

typedef struct QoaDecoder {
    FlacInput input;
    QoaInfo info;
    uint32_t frame_position;
    uint32_t frame_first_sample;
    uint32_t frame_count;
    uint32_t cursor;
    uint32_t pending_skip;
    uint32_t next_sample;
    bool have_frame;
    uint8_t frame[QOA_MAX_FRAME_BYTES];
    int16_t samples[QOA_FRAME_SAMPLES * QOA_MAX_PLAYBACK_CHANNELS];
} QoaDecoder;

QoaResult qoa_parse_header(FlacInput input, QoaInfo *info);
QoaResult qoa_open(QoaDecoder *decoder, FlacInput input);
void qoa_start(QoaDecoder *decoder, FlacInput input, QoaInfo info, uint32_t first_sample);
QoaResult qoa_next_block(QoaDecoder *decoder, const int16_t **left, const int16_t **right,
                         uint32_t *stride, uint32_t *first_sample, uint32_t *samples);
QoaResult qoa_seek(QoaDecoder *decoder, uint32_t sample);
bool qoa_frame_at(FlacInput input, const QoaInfo *info, uint32_t sample, uint32_t *offset,
                  uint32_t *first_sample);

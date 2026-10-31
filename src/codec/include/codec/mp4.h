#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "codec/flac.h"

enum {
    MP4_CONFIG_CAPACITY = 64,
    MP4_CACHE_BYTES = 96,
    MP4_LENGTH_BYTES = 4,
    MP4_OBJECT_AAC_LC = 2
};

typedef enum Mp4Codec {
    MP4_CODEC_NONE,
    MP4_CODEC_ALAC,
    MP4_CODEC_AAC
} Mp4Codec;

typedef struct Mp4Table {
    uint32_t offset;
    uint32_t count;
    uint32_t width;
} Mp4Table;

typedef struct Mp4Track {
    Mp4Codec codec;
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint8_t object_type;
    uint32_t timescale;
    uint32_t frames;
    uint32_t frame_samples;
    uint32_t frame_size;
    Mp4Table sizes;
    Mp4Table chunks;
    Mp4Table offsets;
    Mp4Table times;
    uint32_t lead_trim;
    uint32_t total_samples;
    uint32_t config_bytes;
    uint8_t config[MP4_CONFIG_CAPACITY];
} Mp4Track;

typedef struct Mp4Cache {
    Mp4Table table;
    uint32_t first;
    uint32_t count;
    uint8_t bytes[MP4_CACHE_BYTES];
} Mp4Cache;

typedef struct Mp4Stream {
    Mp4Track track;
    FlacInput file;
    Mp4Cache sizes;
    Mp4Cache chunks;
    Mp4Cache offsets;
    uint32_t frame;
    uint32_t chunk;
    uint32_t chunk_left;
    uint32_t entry;
    uint32_t entry_end;
    uint32_t per_chunk;
    uint32_t file_offset;
    uint32_t frame_bytes;
    uint32_t emitted;
    uint32_t position;
    uint32_t size;
    uint32_t data_bytes;
} Mp4Stream;

bool mp4_signature(const uint8_t *bytes);
bool mp4_probe(FlacInput file, Mp4Track *track);
bool mp4_stream_open(Mp4Stream *stream, FlacInput file, const Mp4Track *track, uint32_t sample,
                     uint32_t *first_sample);
FlacInput mp4_stream_input(Mp4Stream *stream);
uint32_t mp4_record_length(const uint8_t *bytes);

#pragma once

#include <stdint.h>

typedef struct FlacWriterSpec {
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bits;
    uint32_t block_size;
    uint32_t frames;
    uint32_t seed;
} FlacWriterSpec;

typedef struct FlacWriterOutput {
    uint8_t *bytes;
    uint32_t size;
    int32_t *samples;
    uint32_t sample_bytes;
} FlacWriterOutput;

FlacWriterOutput flac_writer_make(FlacWriterSpec spec);
void flac_writer_free(FlacWriterOutput *output);

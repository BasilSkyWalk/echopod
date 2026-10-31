#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/core1_stream.h"

enum {
    CORE1_AAC_FRAME_SAMPLES = 1024,
    CORE1_AAC_MAX_FRAME_BYTES = 6144 / 8 * FLAC_MAX_CHANNELS
};

typedef struct Core1Aac {
    void *handle;
    void (*reset_heap)(void);
    FlacInput input;
    const bool *starved;
    uint32_t position;
    uint32_t next_sample;
    uint32_t frame_samples;
    uint32_t channels;
    bool configured;
    uint8_t frame[CORE1_AAC_MAX_FRAME_BYTES];
} Core1Aac;

Core1Decoder core1_aac_decoder(Core1Aac *aac, void (*reset_heap)(void));
void core1_aac_close(Core1Aac *aac);

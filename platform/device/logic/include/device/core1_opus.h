#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "codec/ogg.h"
#include "device/core1_stream.h"

enum {
    CORE1_OPUS_MAX_FRAME = 2880,
    CORE1_OPUS_PACKET_BYTES = 4096
};

typedef struct Core1OpusMemory {
    void *decoder;
    uint32_t decoder_bytes;
    uint8_t *packet;
    int16_t *output;
} Core1OpusMemory;

typedef struct Core1Opus {
    Core1OpusMemory memory;
    OggReader reader;
    const bool *starved;
    uint32_t channels;
    bool ready;
} Core1Opus;

uint32_t core1_opus_decoder_bytes(void);
Core1Decoder core1_opus_decoder(Core1Opus *opus, Core1OpusMemory memory);

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "codec/alac.h"
#include "device/core1_stream.h"

typedef struct Core1Alac {
    AlacDecoder *decoder;
    FlacInput input;
    const bool *starved;
    uint32_t position;
    uint32_t next_sample;
    bool configured;
} Core1Alac;

Core1Decoder core1_alac_decoder(Core1Alac *alac, AlacDecoder *decoder);

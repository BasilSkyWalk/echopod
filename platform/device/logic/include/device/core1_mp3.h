#pragma once

#include <stdint.h>

#include "device/core1_stream.h"
#include "device/mp3_decoder.h"

typedef struct Core1Mp3 {
    Mp3Decoder *decoder;
    uint32_t next_sample;
} Core1Mp3;

Core1Decoder core1_mp3_decoder(Core1Mp3 *mp3, Mp3Decoder *decoder);

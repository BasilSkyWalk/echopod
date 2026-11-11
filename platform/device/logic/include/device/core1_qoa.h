#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "codec/qoa.h"
#include "device/core1_stream.h"

typedef struct Core1Qoa {
    QoaDecoder *decoder;
    const bool *starved;
} Core1Qoa;

Core1Decoder core1_qoa_decoder(Core1Qoa *qoa, QoaDecoder *decoder);

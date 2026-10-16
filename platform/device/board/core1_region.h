#pragma once

#include <stdint.h>

#include "device/core1_control.h"
#include "device/core1_stream.h"

_Static_assert(CORE1_STREAM_SAMPLES +
                       sizeof(int32_t[STREAM_SLOTS][FLAC_MAX_CHANNELS][FLAC_MAX_BLOCK]) <=
                   CORE1_STREAM_RING,
               "the core 1 output slots must end below the input ring");
_Static_assert(CORE1_STREAM_CONTROL + sizeof(StreamControl) <=
                   CORE1_SHARED_BASE + CORE1_SHARED_BYTES,
               "the stream control block must fit in the shared block");

static inline StreamRegion core1_region(void)
{
    return (StreamRegion){
        .control = (volatile StreamControl *)(uintptr_t)CORE1_STREAM_CONTROL,
        .samples = (int32_t (*)[FLAC_MAX_CHANNELS][FLAC_MAX_BLOCK])(uintptr_t)CORE1_STREAM_SAMPLES,
        .ring = (volatile uint8_t *)(uintptr_t)CORE1_STREAM_RING,
        .ring_bytes = CORE1_STREAM_RING_BYTES,
    };
}

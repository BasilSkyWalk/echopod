#pragma once

#include <stdint.h>

#include "device/core1_control.h"
#include "device/core1_stream.h"

_Static_assert(CORE1_STREAM_SAMPLES + sizeof(int32_t[STREAM_SAMPLE_WORDS]) <= CORE1_STREAM_RING,
               "the core 1 output slots must end below the input ring");
_Static_assert(CORE1_STREAM_CONTROL + sizeof(StreamControl) <=
                   CORE1_SHARED_BASE + CORE1_SHARED_BYTES,
               "the stream control block must fit in the shared block");
_Static_assert(CORE1_FLAC_SLOTS_START +
                       sizeof(int32_t[STREAM_SLOTS][FLAC_MAX_CHANNELS][CORE1_FLAC_SLOT_SAMPLES]) <=
                   CORE1_STREAM_RING,
               "the FLAC program's output slots must end below the input ring");
_Static_assert(CORE1_FLAC_BANK_1 + sizeof(int32_t[FLAC_MAX_BLOCK]) <= CORE1_FLAC_SLOTS_START,
               "the FLAC program's second bank must end below its output slots");
_Static_assert(CORE1_FLAC_BANK_0 + sizeof(int32_t[FLAC_MAX_BLOCK]) <= CORE1_SHARED_BASE,
               "the FLAC program's first bank must end below the shared block");

static inline StreamRegion core1_region(void)
{
    return (StreamRegion){
        .control = (volatile StreamControl *)(uintptr_t)CORE1_STREAM_CONTROL,
        .samples = (int32_t *)(uintptr_t)CORE1_STREAM_SAMPLES,
        .ring = (volatile uint8_t *)(uintptr_t)CORE1_STREAM_RING,
        .ring_bytes = CORE1_STREAM_RING_BYTES,
    };
}

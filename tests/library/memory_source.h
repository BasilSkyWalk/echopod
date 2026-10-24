#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "library/byte_source.h"

typedef struct MemorySource {
    const uint8_t *bytes;
    uint32_t size;
    int reads;
    bool failing;
} MemorySource;

ByteSource memory_source(MemorySource *memory);

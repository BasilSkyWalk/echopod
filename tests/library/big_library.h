#pragma once

#include <stdint.h>

#include "library/library.h"
#include "memory_source.h"

typedef struct BigLibrary {
    uint8_t *bytes;
    uint32_t size;
    MemorySource memory;
    Library library;
} BigLibrary;

Library *big_library_open(BigLibrary *big, uint32_t tracks, uint32_t tracks_per_album);
void big_library_close(BigLibrary *big);

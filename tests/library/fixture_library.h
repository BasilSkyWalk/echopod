#pragma once

#include <stdint.h>

#include "library/library.h"
#include "memory_source.h"

enum {
    FIXTURE_TRACKS = 8,
    FIXTURE_ALBUMS = 5,
    FIXTURE_GROUPS = 4,
    FIXTURE_FOLDERS = 10,
    FIXTURE_CAPACITY = 8192,
    FIXTURE_COVERS_CAPACITY = 65536
};

typedef struct FixtureLibrary {
    uint8_t bytes[FIXTURE_CAPACITY];
    uint32_t size;
    MemorySource memory;
    Library library;
} FixtureLibrary;

const uint8_t *fixture_bytes(uint32_t *size);
const uint8_t *fixture_covers_bytes(uint32_t *size);
Library *fixture_library_open(FixtureLibrary *fixture);

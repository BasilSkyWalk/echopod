#include "fixture_library.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t fixture_data[FIXTURE_CAPACITY];
static uint32_t fixture_size;

const uint8_t *fixture_bytes(uint32_t *size)
{
    if (fixture_size == 0) {
        FILE *file = fopen(ECHO_FIXTURE_DIR "/small_library.db", "rb");
        if (file == NULL) {
            fprintf(stderr, "missing fixture %s\n", ECHO_FIXTURE_DIR "/small_library.db");
            exit(1);
        }
        fixture_size = (uint32_t)fread(fixture_data, 1, sizeof fixture_data, file);
        fclose(file);
    }
    *size = fixture_size;
    return fixture_data;
}

Library *fixture_library_open(FixtureLibrary *fixture)
{
    uint32_t size = 0;
    const uint8_t *bytes = fixture_bytes(&size);
    memcpy(fixture->bytes, bytes, size);
    fixture->size = size;
    fixture->memory = (MemorySource){.bytes = fixture->bytes, .size = size};
    library_open(&fixture->library, memory_source(&fixture->memory), size);
    return &fixture->library;
}

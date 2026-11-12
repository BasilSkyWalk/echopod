#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "library/byte_source.h"

enum {
    COVER_FULL_SIZE = 112,
    COVER_HALF_SIZE = 56,
    COVER_FULL_PIXELS = COVER_FULL_SIZE * COVER_FULL_SIZE,
    COVER_HALF_PIXELS = COVER_HALF_SIZE * COVER_HALF_SIZE,
    COVERS_VERSION = 1,
    COVERS_HEADER_SIZE = 16,
    COVERS_ENTRY_SIZE = 8,
    COVERS_TOP_NODES = 64,
    COVERS_CHUNK_ENTRIES = 64
};

#define ECHO_MINI_DEFAULT_COVER 0xFFFFFFFFu

typedef enum CoverSize {
    COVER_FULL,
    COVER_HALF
} CoverSize;

typedef struct Covers {
    ByteSource source;
    uint32_t size;
    uint32_t count;
    uint64_t known_nodes;
    uint32_t top_keys[COVERS_TOP_NODES];
} Covers;

bool covers_open(Covers *covers, ByteSource source, uint32_t size);
bool covers_find(Covers *covers, uint32_t key, uint32_t *record);
bool covers_read(const Covers *covers, uint32_t record, CoverSize size, uint16_t *pixels);

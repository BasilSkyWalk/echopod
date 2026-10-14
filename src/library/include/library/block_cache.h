#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "library/byte_source.h"

enum {
    BLOCK_CACHE_BLOCK_SIZE = 512,
    BLOCK_CACHE_BLOCK_COUNT = 4
};

typedef struct CachedBlock {
    uint32_t number;
    uint32_t last_used;
    bool valid;
    uint8_t bytes[BLOCK_CACHE_BLOCK_SIZE];
} CachedBlock;

typedef struct BlockCache {
    ByteSource source;
    uint32_t source_size;
    uint32_t clock;
    CachedBlock blocks[BLOCK_CACHE_BLOCK_COUNT];
} BlockCache;

void block_cache_init(BlockCache *cache, ByteSource source, uint32_t source_size);
bool block_cache_read(BlockCache *cache, uint32_t offset, void *buffer, uint32_t length);
ByteSource block_cache_source(BlockCache *cache);

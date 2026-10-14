#include "library/block_cache.h"

#include <stddef.h>

static uint32_t min_of(uint32_t first, uint32_t second)
{
    return first < second ? first : second;
}

static void copy_bytes(uint8_t *target, const uint8_t *source, uint32_t length)
{
    for (uint32_t index = 0; index < length; index++) {
        target[index] = source[index];
    }
}

static CachedBlock *find_block(BlockCache *cache, uint32_t number)
{
    for (int index = 0; index < BLOCK_CACHE_BLOCK_COUNT; index++) {
        CachedBlock *block = &cache->blocks[index];
        if (block->valid && block->number == number) {
            return block;
        }
    }
    return NULL;
}

static CachedBlock *least_recently_used(BlockCache *cache)
{
    CachedBlock *oldest = &cache->blocks[0];
    for (int index = 0; index < BLOCK_CACHE_BLOCK_COUNT; index++) {
        CachedBlock *block = &cache->blocks[index];
        if (!block->valid) {
            return block;
        }
        if (block->last_used < oldest->last_used) {
            oldest = block;
        }
    }
    return oldest;
}

static CachedBlock *load_block(BlockCache *cache, uint32_t number)
{
    CachedBlock *block = find_block(cache, number);
    if (block == NULL) {
        block = least_recently_used(cache);
        block->valid = false;
        uint32_t start = number * BLOCK_CACHE_BLOCK_SIZE;
        uint32_t length = min_of(BLOCK_CACHE_BLOCK_SIZE, cache->source_size - start);
        if (!cache->source.read(cache->source.context, start, block->bytes, length)) {
            return NULL;
        }
        block->number = number;
        block->valid = true;
    }
    block->last_used = ++cache->clock;
    return block;
}

static bool read_through_cache(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    return block_cache_read(context, offset, buffer, length);
}

void block_cache_init(BlockCache *cache, ByteSource source, uint32_t source_size)
{
    *cache = (BlockCache){.source = source, .source_size = source_size};
}

bool block_cache_read(BlockCache *cache, uint32_t offset, void *buffer, uint32_t length)
{
    if (offset > cache->source_size || length > cache->source_size - offset) {
        return false;
    }
    uint8_t *output = buffer;
    while (length > 0) {
        uint32_t within = offset % BLOCK_CACHE_BLOCK_SIZE;
        uint32_t chunk = min_of(length, BLOCK_CACHE_BLOCK_SIZE - within);
        CachedBlock *block = load_block(cache, offset / BLOCK_CACHE_BLOCK_SIZE);
        if (block == NULL) {
            return false;
        }
        copy_bytes(output, block->bytes + within, chunk);
        output += chunk;
        offset += chunk;
        length -= chunk;
    }
    return true;
}

ByteSource block_cache_source(BlockCache *cache)
{
    return (ByteSource){.read = read_through_cache, .context = cache};
}

#include <string.h>

#include "check.h"
#include "library/block_cache.h"
#include "memory_source.h"

enum {
    DATA_SIZE = 3000
};

static uint8_t data[DATA_SIZE];
static MemorySource memory;
static BlockCache cache;

static BlockCache *fresh_cache(uint32_t size)
{
    for (uint32_t index = 0; index < DATA_SIZE; index++) {
        data[index] = (uint8_t)(index * 7 + 3);
    }
    memory = (MemorySource){.bytes = data, .size = size};
    block_cache_init(&cache, memory_source(&memory), size);
    return &cache;
}

static bool matches_data(const uint8_t *buffer, uint32_t offset, uint32_t length)
{
    return memcmp(buffer, data + offset, length) == 0;
}

static void reads_return_the_source_bytes(void)
{
    uint8_t buffer[16];
    CHECK(block_cache_read(fresh_cache(DATA_SIZE), 10, buffer, sizeof buffer));
    CHECK(matches_data(buffer, 10, sizeof buffer));
}

static void reads_span_block_boundaries(void)
{
    uint8_t buffer[700];
    CHECK(block_cache_read(fresh_cache(DATA_SIZE), 400, buffer, sizeof buffer));
    CHECK(matches_data(buffer, 400, sizeof buffer));
}

static void repeated_reads_come_from_the_cache(void)
{
    uint8_t buffer[8];
    BlockCache *blocks = fresh_cache(DATA_SIZE);
    block_cache_read(blocks, 20, buffer, sizeof buffer);
    block_cache_read(blocks, 100, buffer, sizeof buffer);
    block_cache_read(blocks, 300, buffer, sizeof buffer);
    CHECK_EQUAL(1, memory.reads);
}

static void the_least_recently_used_block_is_evicted(void)
{
    uint8_t byte;
    BlockCache *blocks = fresh_cache(DATA_SIZE);
    for (uint32_t block = 0; block < BLOCK_CACHE_BLOCK_COUNT; block++) {
        block_cache_read(blocks, block * BLOCK_CACHE_BLOCK_SIZE, &byte, 1);
    }
    block_cache_read(blocks, 0, &byte, 1);
    block_cache_read(blocks, BLOCK_CACHE_BLOCK_COUNT * BLOCK_CACHE_BLOCK_SIZE, &byte, 1);
    int reads_before = memory.reads;
    block_cache_read(blocks, 0, &byte, 1);
    CHECK_EQUAL(reads_before, memory.reads);
    block_cache_read(blocks, BLOCK_CACHE_BLOCK_SIZE, &byte, 1);
    CHECK_EQUAL(reads_before + 1, memory.reads);
}

static void the_last_partial_block_is_readable(void)
{
    uint8_t buffer[4];
    CHECK(block_cache_read(fresh_cache(1000), 996, buffer, sizeof buffer));
    CHECK(matches_data(buffer, 996, sizeof buffer));
}

static void reads_past_the_end_fail(void)
{
    uint8_t buffer[8];
    BlockCache *blocks = fresh_cache(1000);
    CHECK(!block_cache_read(blocks, 996, buffer, sizeof buffer));
    CHECK(!block_cache_read(blocks, 5000, buffer, 1));
}

static void source_failures_are_reported_and_not_cached(void)
{
    uint8_t byte;
    BlockCache *blocks = fresh_cache(DATA_SIZE);
    memory.failing = true;
    CHECK(!block_cache_read(blocks, 0, &byte, 1));
    memory.failing = false;
    CHECK(block_cache_read(blocks, 0, &byte, 1));
    CHECK(matches_data(&byte, 0, 1));
}

static void the_cache_is_itself_a_byte_source(void)
{
    uint8_t buffer[16];
    ByteSource source = block_cache_source(fresh_cache(DATA_SIZE));
    CHECK(source.read(source.context, 600, buffer, sizeof buffer));
    CHECK(matches_data(buffer, 600, sizeof buffer));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(reads_return_the_source_bytes),
        TEST_CASE(reads_span_block_boundaries),
        TEST_CASE(repeated_reads_come_from_the_cache),
        TEST_CASE(the_least_recently_used_block_is_evicted),
        TEST_CASE(the_last_partial_block_is_readable),
        TEST_CASE(reads_past_the_end_fail),
        TEST_CASE(source_failures_are_reported_and_not_cached),
        TEST_CASE(the_cache_is_itself_a_byte_source),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

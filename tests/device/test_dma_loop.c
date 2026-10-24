#include "check.h"
#include "device/dma_loop.h"

static void the_item_feeds_txdr_from_the_buffer_and_points_at_itself(void)
{
    DmaItem item = dma_loop_item(0x03010000u, 4000, 0x03020000u);
    CHECK_EQUAL(0x03010000u, item.source);
    CHECK_EQUAL(0x40030024u, item.destination);
    CHECK_EQUAL(0x03020000u, item.next);
    CHECK_EQUAL(0x18909124u, item.control_low);
    CHECK_EQUAL(4000u, item.control_high);
    CHECK_EQUAL(0u, item.unused);
}

static void the_position_counts_words_from_the_buffer_start(void)
{
    CHECK_EQUAL(0u, dma_loop_position(0x03010000u, 0x03010000u, 4000));
    CHECK_EQUAL(1234u, dma_loop_position(0x03010000u + 4 * 1234, 0x03010000u, 4000));
    CHECK_EQUAL(0u, dma_loop_position(0x03000000u, 0x03010000u, 4000));
    CHECK_EQUAL(0u, dma_loop_position(0x03010000u + 4 * 5000, 0x03010000u, 4000));
}

static void a_ring_chains_its_blocks_in_a_loop(void)
{
    DmaItem items[4];
    CHECK(dma_ring_items(items, 4, 0x03010000u, 4000, 0x03020000u));
    for (uint32_t index = 0; index < 4; index++) {
        CHECK_EQUAL(0x03010000u + index * 4000u * 4u, items[index].source);
        CHECK_EQUAL(0x40030024u, items[index].destination);
        CHECK_EQUAL(0x18909124u, items[index].control_low);
        CHECK_EQUAL(4000u, items[index].control_high);
    }
    CHECK_EQUAL(0x03020000u + 1u * sizeof(DmaItem), items[0].next);
    CHECK_EQUAL(0x03020000u + 3u * sizeof(DmaItem), items[2].next);
    CHECK_EQUAL(0x03020000u, items[3].next);
}

static void blocks_larger_than_the_controller_allows_are_refused(void)
{
    DmaItem items[1];
    CHECK(dma_ring_items(items, 1, 0x03010000u, DMA_MAX_BLOCK_WORDS, 0x03020000u));
    CHECK(!dma_ring_items(items, 1, 0x03010000u, DMA_MAX_BLOCK_WORDS + 1, 0x03020000u));
    CHECK(!dma_ring_items(items, 0, 0x03010000u, 4000, 0x03020000u));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_item_feeds_txdr_from_the_buffer_and_points_at_itself),
        TEST_CASE(the_position_counts_words_from_the_buffer_start),
        TEST_CASE(a_ring_chains_its_blocks_in_a_loop),
        TEST_CASE(blocks_larger_than_the_controller_allows_are_refused),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

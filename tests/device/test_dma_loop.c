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

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_item_feeds_txdr_from_the_buffer_and_points_at_itself),
        TEST_CASE(the_position_counts_words_from_the_buffer_start),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

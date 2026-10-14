#pragma once

#include <stdint.h>

typedef struct DmaItem {
    uint32_t source;
    uint32_t destination;
    uint32_t next;
    uint32_t control_low;
    uint32_t control_high;
    uint32_t unused;
} DmaItem;

DmaItem dma_loop_item(uint32_t buffer, uint32_t words, uint32_t item_address);
uint32_t dma_loop_position(uint32_t source, uint32_t buffer, uint32_t words);

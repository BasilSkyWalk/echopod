#include "device/dma_loop.h"

#define I2S1_TXDR 0x40030024u
#define CONTROL_LOW_LOOPED 0x18909124u

enum {
    WORD_BYTES = 4
};

DmaItem dma_loop_item(uint32_t buffer, uint32_t words, uint32_t item_address)
{
    return (DmaItem){
        .source = buffer,
        .destination = I2S1_TXDR,
        .next = item_address,
        .control_low = CONTROL_LOW_LOOPED,
        .control_high = words,
        .unused = 0,
    };
}

uint32_t dma_loop_position(uint32_t source, uint32_t buffer, uint32_t words)
{
    if (source < buffer) {
        return 0;
    }
    uint32_t position = (source - buffer) / WORD_BYTES;
    return position < words ? position : 0;
}

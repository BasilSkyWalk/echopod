#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/block_device.h"

typedef struct SparseCard {
    uint8_t *bytes;
    uint32_t sector_count;
    uint32_t reads;
    uint32_t calls;
    uint32_t writes;
    uint32_t write_calls;
} SparseCard;

bool sparse_card_load(SparseCard *card, const char *name);
void sparse_card_free(SparseCard *card);
BlockDevice sparse_card_device(SparseCard *card);

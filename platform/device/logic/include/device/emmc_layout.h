#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/block_device.h"

typedef struct EmmcLayout {
    uint32_t slot_1;
    uint32_t slot_2;
    uint32_t slot_sectors;
    uint32_t firmware_end;
    uint32_t volume_start;
    uint32_t volume_sectors;
} EmmcLayout;

bool emmc_layout_read(EmmcLayout *layout, const BlockDevice *device, uint32_t total_sectors);
bool emmc_layout_allows_write(const EmmcLayout *layout, uint32_t lba, uint32_t count);

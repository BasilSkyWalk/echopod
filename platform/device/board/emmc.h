#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/block_device.h"
#include "device/emmc_layout.h"

typedef struct EmmcInfo {
    uint32_t sector_count;
    uint32_t serial;
    EmmcLayout layout;
} EmmcInfo;

bool emmc_start(EmmcInfo *info);
bool emmc_read(void *context, uint32_t lba, uint32_t count, void *buffer);
bool emmc_write(void *context, uint32_t lba, uint32_t count, const void *buffer);
BlockDevice emmc_device(void);
bool emmc_volume_read(void *context, uint32_t lba, uint32_t count, void *buffer);
bool emmc_volume_write(void *context, uint32_t lba, uint32_t count, const void *buffer);

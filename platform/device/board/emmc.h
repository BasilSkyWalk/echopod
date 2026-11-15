#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/block_device.h"
#include "device/emmc_layout.h"

typedef struct EmmcInfo {
    uint32_t sector_count;
    uint32_t serial;
    EmmcLayout layout;
    bool sd_nand;
} EmmcInfo;

typedef struct EmmcProbe {
    uint32_t started;
    uint32_t sd_nand;
    uint32_t sector_count;
    uint32_t volume_start;
    uint32_t status;
    uint32_t lba0_ok;
    uint32_t lba0_0;
    uint32_t lba0_1;
    uint32_t head_64m;
    uint32_t head_256m;
} EmmcProbe;

bool emmc_start(EmmcInfo *info);
EmmcProbe emmc_probe(void);
bool emmc_status(uint32_t *status);
bool emmc_read(void *context, uint32_t lba, uint32_t count, void *buffer);
bool emmc_write(void *context, uint32_t lba, uint32_t count, const void *buffer);
BlockDevice emmc_device(void);
uint32_t emmc_fifo_depth_words(void);
bool emmc_volume_read(void *context, uint32_t lba, uint32_t count, void *buffer);
bool emmc_volume_write(void *context, uint32_t lba, uint32_t count, const void *buffer);

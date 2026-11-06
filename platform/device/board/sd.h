#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/block_device.h"
#include "device/sd_card.h"
#include "mmc.h"

typedef struct SdHealth {
    SdRetryStats retry;
    MmcFailure failure;
    uint32_t failures;
    uint32_t starts;
} SdHealth;

bool sd_start(BlockDevice *device);
bool sd_prepare(BlockDevice *device);
SdHealth sd_health(void);
uint32_t sd_sector_count(void);
bool sd_read(void *context, uint32_t lba, uint32_t count, void *buffer);
bool sd_write(void *context, uint32_t lba, uint32_t count, const void *buffer);

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/block_device.h"

bool sd_start(BlockDevice *device);
uint32_t sd_sector_count(void);
bool sd_read(void *context, uint32_t lba, uint32_t count, void *buffer);

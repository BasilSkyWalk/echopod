#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/block_device.h"

enum {
    FAKE_DISK_SECTORS = 32
};

typedef struct FakeDisk {
    uint32_t sector_count;
    uint32_t lbas[FAKE_DISK_SECTORS];
    uint8_t sectors[FAKE_DISK_SECTORS][512];
    int used;
    uint32_t reads;
} FakeDisk;

void fake_disk_init(FakeDisk *disk, uint32_t sector_count);
uint8_t *fake_disk_sector(FakeDisk *disk, uint32_t lba);
BlockDevice fake_disk_device(FakeDisk *disk);

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "device/fat.h"

typedef struct FatNewEntry {
    const char *name;
    size_t length;
    bool directory;
    uint32_t size;
} FatNewEntry;

enum {
    FAT_NEW_DATE_YEAR = 2026,
    FAT_NEW_DATE_MONTH = 1,
    FAT_NEW_DATE_DAY = 1
};

uint32_t fat_new_cluster_count(const FatVolume *volume, const FatNewEntry *entry);
uint32_t fat_new_cleared_sectors(const FatVolume *volume, const FatNewEntry *entry);
uint8_t fat_upper_ascii(char character);
FatReserveResult fat_create_entry(FatVolume *volume, FatFile *parent, const FatNewEntry *entry,
                                  FatFile *created);
FatReserveResult exfat_create_entry(FatVolume *volume, FatFile *parent, const FatNewEntry *entry,
                                    FatFile *created);

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

typedef struct FatGrowth {
    bool (*is_free)(FatVolume *volume, void *context, uint32_t cluster, bool *free);
    bool (*claim)(FatVolume *volume, void *context, uint32_t first, uint32_t count);
    void *context;
    uint32_t start;
    uint32_t needed;
    uint32_t last;
} FatGrowth;

uint32_t fat_clusters_for(const FatVolume *volume, uint32_t size);
bool fat_write_chain(FatVolume *volume, uint32_t first, uint32_t count);
bool fat_link_cluster(FatVolume *volume, uint32_t cluster, uint32_t next);
uint32_t fat_last_cluster(FatVolume *volume, FatFile file, uint32_t clusters);
FatReserveResult fat_grow_chain(FatVolume *volume, const FatGrowth *growth, uint32_t *final);
FatReserveResult fat_grow_entry(FatVolume *volume, FatFile *parent, uint32_t index, FatFile file,
                                uint32_t size);
FatReserveResult exfat_grow_entry(FatVolume *volume, FatFile *parent, uint32_t index, FatFile file,
                                  uint32_t size);

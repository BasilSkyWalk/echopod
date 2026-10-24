#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "device/fat.h"

uint16_t fat_le16(const uint8_t *bytes);
uint32_t fat_le32(const uint8_t *bytes);
const uint8_t *fat_cached_sector(const FatVolume *volume);
bool fat_read_sector(FatVolume *volume, uint32_t lba);
bool fat_sector_cached(FatVolume *volume, uint32_t lba);
bool fat_read_run(FatVolume *volume, uint32_t lba, uint32_t count);
uint32_t fat_run_length(FatVolume *volume, FatFile *file, uint32_t sector_index, uint32_t limit);
uint8_t *fat_sector_for_update(FatVolume *volume, uint32_t lba);
bool fat_commit_sector(FatVolume *volume, uint32_t lba);
bool fat_clear_sectors(FatVolume *volume, uint32_t lba, uint32_t count);
bool fat_update_bytes(FatVolume *volume, uint32_t lba, uint32_t within, const uint8_t *bytes,
                      uint32_t length);
uint32_t fat_cluster_lba(const FatVolume *volume, uint32_t cluster);
bool fat_find_entry(FatVolume *volume, FatFile *directory, const char *name, size_t length,
                    FatFile *found);
FatFile fat_root_directory(const FatVolume *volume);
const uint8_t *fat_read_entry(FatVolume *volume, FatFile *directory, uint32_t index);

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/fat.h"

uint16_t fat_le16(const uint8_t *bytes);
uint32_t fat_le32(const uint8_t *bytes);
const uint8_t *fat_cached_sector(const FatVolume *volume);
bool fat_read_sector(FatVolume *volume, uint32_t lba);
bool fat_file_sector(FatVolume *volume, FatFile *file, uint32_t sector_index, uint32_t *lba);
FatFile fat_root_directory(const FatVolume *volume);
const uint8_t *fat_read_entry(FatVolume *volume, FatFile *directory, uint32_t index);

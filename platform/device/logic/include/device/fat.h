#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/block_device.h"

enum {
    FAT_SECTOR_SIZE = 512,
    FAT_CACHE_SECTORS = 8,
    FAT_LONG_NAME_CAPACITY = 260,
    FAT_NAME_CAPACITY = 768
};

typedef enum FatType {
    FAT_TYPE_16 = 16,
    FAT_TYPE_32 = 32,
    FAT_TYPE_EXFAT = 64
} FatType;

typedef struct FatFile {
    uint32_t first_cluster;
    uint32_t size;
    uint32_t cursor_index;
    uint32_t cursor_cluster;
    bool directory;
    bool contiguous;
} FatFile;

typedef struct FatVolume {
    BlockDevice device;
    FatType type;
    uint32_t volume_start;
    uint32_t fat_start;
    uint32_t root_start;
    uint32_t root_sectors;
    uint32_t root_cluster;
    uint32_t data_start;
    uint32_t cluster_count;
    uint32_t cluster_shift;
    uint32_t cache_lba;
    uint32_t cache_count;
    uint32_t cache_index;
    uint32_t fat_sector_lba;
    bool fat_sector_valid;
    uint32_t cache[FAT_CACHE_SECTORS * FAT_SECTOR_SIZE / 4];
    uint32_t fat_sector[FAT_SECTOR_SIZE / 4];
    uint16_t long_name[FAT_LONG_NAME_CAPACITY];
} FatVolume;

typedef struct FatDirCursor {
    FatFile directory;
    uint32_t index;
} FatDirCursor;

typedef struct FatDirEntry {
    FatFile file;
    bool hidden;
    char name[FAT_NAME_CAPACITY];
} FatDirEntry;

bool fat_mount(FatVolume *volume, BlockDevice device);
bool fat_open_directory(FatVolume *volume, const char *path, FatDirCursor *cursor);
bool fat_next_entry(FatVolume *volume, FatDirCursor *cursor, FatDirEntry *entry);
bool fat_open(FatVolume *volume, const char *path, FatFile *file);
bool fat_read(FatVolume *volume, FatFile *file, uint32_t offset, void *buffer, uint32_t length);
bool fat_write(FatVolume *volume, FatFile *file, uint32_t offset, const void *bytes,
               uint32_t length);

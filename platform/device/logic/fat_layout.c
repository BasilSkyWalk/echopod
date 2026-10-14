#include "fat_layout.h"

#define FAT32_CLUSTER_MASK 0x0FFFFFFFu

enum {
    BOOT_SIGNATURE = 0xAA55,
    SIGNATURE_OFFSET = 510,
    PARTITION_ENTRY = 0x1BE,
    FIRST_CLUSTER = 2,
    FAT16_MINIMUM_CLUSTERS = 4085,
    FAT32_MINIMUM_CLUSTERS = 65525,
    DIRECTORY_ENTRY_SIZE = 32
};

uint16_t fat_le16(const uint8_t *bytes)
{
    return (uint16_t)(bytes[0] | bytes[1] << 8);
}

uint32_t fat_le32(const uint8_t *bytes)
{
    return (uint32_t)fat_le16(bytes) | (uint32_t)fat_le16(bytes + 2) << 16;
}

const uint8_t *fat_cached_sector(const FatVolume *volume)
{
    return (const uint8_t *)volume->sector;
}

bool fat_read_sector(FatVolume *volume, uint32_t lba)
{
    if (volume->sector_valid && volume->sector_lba == lba) {
        return true;
    }
    volume->sector_lba = lba;
    volume->sector_valid = volume->device.read(volume->device.context, lba, 1, volume->sector);
    return volume->sector_valid;
}

static bool is_boot_sector(const uint8_t *sector)
{
    uint8_t per_cluster = sector[13];
    return fat_le16(sector + SIGNATURE_OFFSET) == BOOT_SIGNATURE &&
           (sector[0] == 0xEB || sector[0] == 0xE9) && fat_le16(sector + 11) == FAT_SECTOR_SIZE &&
           per_cluster != 0 && (per_cluster & (per_cluster - 1)) == 0 && sector[16] != 0;
}

static bool partition_start(const uint8_t *sector, uint32_t *start)
{
    const uint8_t *entry = sector + PARTITION_ENTRY;
    if (fat_le16(sector + SIGNATURE_OFFSET) != BOOT_SIGNATURE ||
        (entry[0] != 0x00 && entry[0] != 0x80) || entry[4] == 0) {
        return false;
    }
    *start = fat_le32(entry + 8);
    return *start != 0;
}

static uint32_t log2_of(uint32_t value)
{
    uint32_t shift = 0;
    while ((1u << shift) < value) {
        shift++;
    }
    return shift;
}

static bool read_layout(FatVolume *volume, const uint8_t *boot)
{
    uint32_t total = fat_le16(boot + 19) != 0 ? fat_le16(boot + 19) : fat_le32(boot + 32);
    uint32_t fat_size = fat_le16(boot + 22) != 0 ? fat_le16(boot + 22) : fat_le32(boot + 36);
    uint32_t root_entries = fat_le16(boot + 17);
    volume->root_sectors =
        (root_entries * DIRECTORY_ENTRY_SIZE + FAT_SECTOR_SIZE - 1) / FAT_SECTOR_SIZE;
    volume->fat_start = volume->volume_start + fat_le16(boot + 14);
    volume->root_start = volume->fat_start + boot[16] * fat_size;
    volume->data_start = volume->root_start + volume->root_sectors;
    volume->cluster_shift = log2_of(boot[13]);
    uint32_t used = volume->data_start - volume->volume_start;
    if (total <= used) {
        return false;
    }
    volume->cluster_count = (total - used) >> volume->cluster_shift;
    volume->root_cluster = fat_le32(boot + 44);
    volume->type = volume->cluster_count < FAT32_MINIMUM_CLUSTERS ? FAT_TYPE_16 : FAT_TYPE_32;
    return volume->cluster_count >= FAT16_MINIMUM_CLUSTERS &&
           (volume->type == FAT_TYPE_32 || root_entries != 0);
}

bool fat_mount(FatVolume *volume, BlockDevice device)
{
    *volume = (FatVolume){.device = device};
    uint32_t start = 0;
    if (!fat_read_sector(volume, 0)) {
        return false;
    }
    if (!is_boot_sector(fat_cached_sector(volume))) {
        if (!partition_start(fat_cached_sector(volume), &start) ||
            !fat_read_sector(volume, start) || !is_boot_sector(fat_cached_sector(volume))) {
            return false;
        }
    }
    volume->volume_start = start;
    return read_layout(volume, fat_cached_sector(volume));
}

static bool read_fat_sector(FatVolume *volume, uint32_t lba)
{
    if (volume->fat_sector_valid && volume->fat_sector_lba == lba) {
        return true;
    }
    volume->fat_sector_lba = lba;
    volume->fat_sector_valid =
        volume->device.read(volume->device.context, lba, 1, volume->fat_sector);
    return volume->fat_sector_valid;
}

static bool next_cluster(FatVolume *volume, uint32_t cluster, uint32_t *next)
{
    uint32_t entry_size = volume->type == FAT_TYPE_32 ? 4u : 2u;
    uint32_t offset = cluster * entry_size;
    if (!read_fat_sector(volume, volume->fat_start + offset / FAT_SECTOR_SIZE)) {
        return false;
    }
    const uint8_t *bytes = (const uint8_t *)volume->fat_sector + offset % FAT_SECTOR_SIZE;
    *next = volume->type == FAT_TYPE_32 ? fat_le32(bytes) & FAT32_CLUSTER_MASK : fat_le16(bytes);
    return *next >= FIRST_CLUSTER && *next < volume->cluster_count + FIRST_CLUSTER;
}

static bool cluster_at(FatVolume *volume, FatFile *file, uint32_t index, uint32_t *cluster)
{
    if (file->cursor_cluster == 0 || index < file->cursor_index) {
        file->cursor_index = 0;
        file->cursor_cluster = file->first_cluster;
    }
    if (index >= volume->cluster_count) {
        return false;
    }
    while (file->cursor_index < index) {
        if (!next_cluster(volume, file->cursor_cluster, &file->cursor_cluster)) {
            file->cursor_cluster = 0;
            return false;
        }
        file->cursor_index++;
    }
    *cluster = file->cursor_cluster;
    return *cluster >= FIRST_CLUSTER;
}

static bool is_fixed_root(const FatFile *file)
{
    return file->directory && file->first_cluster == 0;
}

bool fat_file_sector(FatVolume *volume, FatFile *file, uint32_t sector_index, uint32_t *lba)
{
    if (is_fixed_root(file)) {
        *lba = volume->root_start + sector_index;
        return sector_index < volume->root_sectors;
    }
    uint32_t cluster = 0;
    if (!cluster_at(volume, file, sector_index >> volume->cluster_shift, &cluster)) {
        return false;
    }
    uint32_t within = sector_index & ((1u << volume->cluster_shift) - 1u);
    *lba = volume->data_start + ((cluster - FIRST_CLUSTER) << volume->cluster_shift) + within;
    return true;
}

FatFile fat_root_directory(const FatVolume *volume)
{
    uint32_t first = volume->type == FAT_TYPE_32 ? volume->root_cluster : 0;
    return (FatFile){.first_cluster = first, .size = UINT32_MAX, .directory = true};
}

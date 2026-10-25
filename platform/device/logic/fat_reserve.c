#include "device/fat.h"

#include "fat_create.h"
#include "fat_layout.h"

static const char FORBIDDEN[] = "\"*/:<>?\\|";

static bool allowed_character(char character)
{
    if (character < 0x20 || character > 0x7E) {
        return false;
    }
    for (const char *forbidden = FORBIDDEN; *forbidden != '\0'; forbidden++) {
        if (character == *forbidden) {
            return false;
        }
    }
    return true;
}

static bool name_valid(const char *name, size_t length)
{
    if (length == 0 || length > FAT_RESERVE_NAME_LIMIT || name[length - 1] == '.' ||
        name[length - 1] == ' ') {
        return false;
    }
    for (size_t index = 0; index < length; index++) {
        if (!allowed_character(name[index])) {
            return false;
        }
    }
    return true;
}

uint8_t fat_upper_ascii(char character)
{
    return (uint8_t)(character >= 'a' && character <= 'z' ? character - 'a' + 'A' : character);
}

uint32_t fat_new_cluster_count(const FatVolume *volume, const FatNewEntry *entry)
{
    uint32_t cluster_bytes = (uint32_t)FAT_SECTOR_SIZE << volume->cluster_shift;
    return entry->directory ? 1u : (entry->size - 1u) / cluster_bytes + 1u;
}

uint32_t fat_new_cleared_sectors(const FatVolume *volume, const FatNewEntry *entry)
{
    if (entry->directory) {
        return 1u << volume->cluster_shift;
    }
    uint32_t cleared =
        entry->size < FAT_RESERVE_CLEARED_BYTES ? entry->size : FAT_RESERVE_CLEARED_BYTES;
    return (cleared + FAT_SECTOR_SIZE - 1) / FAT_SECTOR_SIZE;
}

static size_t component_length(const char *path)
{
    size_t length = 0;
    while (path[length] != '\0' && path[length] != '/') {
        length++;
    }
    return length;
}

static FatReserveResult create(FatVolume *volume, FatFile *parent, const FatNewEntry *entry,
                               FatFile *created)
{
    if (!name_valid(entry->name, entry->length)) {
        return FAT_RESERVE_FAILED;
    }
    if (volume->type == FAT_TYPE_EXFAT) {
        return exfat_create_entry(volume, parent, entry, created);
    }
    return fat_create_entry(volume, parent, entry, created);
}

static FatReserveResult step(FatVolume *volume, FatFile *directory, const FatNewEntry *entry)
{
    FatFile found;
    bool last = !entry->directory;
    if (!fat_find_entry(volume, directory, entry->name, entry->length, &found)) {
        FatReserveResult result = create(volume, directory, entry, &found);
        if (result != FAT_RESERVE_CREATED || last) {
            return result;
        }
    } else if (last) {
        return FAT_RESERVE_EXISTS;
    } else if (!found.directory) {
        return FAT_RESERVE_FAILED;
    }
    found.cursor_index = 0;
    found.cursor_cluster = 0;
    *directory = found;
    return FAT_RESERVE_EXISTS;
}

FatReserveResult fat_reserve(FatVolume *volume, const char *path, uint32_t size)
{
    volume->parent_valid = false;
    if (size == 0 || volume->device.write == NULL) {
        return FAT_RESERVE_FAILED;
    }
    FatFile directory = fat_root_directory(volume);
    while (*path == '/') {
        path++;
    }
    while (*path != '\0') {
        size_t length = component_length(path);
        const char *rest = path + length;
        while (*rest == '/') {
            rest++;
        }
        FatNewEntry entry = {path, length, *rest != '\0', size};
        FatReserveResult result = step(volume, &directory, &entry);
        if (!entry.directory || result != FAT_RESERVE_EXISTS) {
            return result;
        }
        path = rest;
    }
    return FAT_RESERVE_FAILED;
}

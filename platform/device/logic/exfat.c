#include "exfat.h"

#include "fat_layout.h"
#include "fat_names.h"

enum {
    ENTRY_END = 0x00,
    ENTRY_FILE = 0x85,
    ENTRY_STREAM = 0xC0,
    ENTRY_NAME = 0xC1,
    ATTRIBUTE_DIRECTORY = 0x10,
    FLAG_NO_FAT_CHAIN = 0x02,
    NAME_UNITS_PER_ENTRY = 15,
    MINIMUM_SECONDARIES = 2,
    MAXIMUM_SECONDARIES = 18,
    MAXIMUM_DIRECTORY_ENTRIES = 0x100000
};

typedef struct EntrySet {
    uint32_t first_cluster;
    uint32_t valid_low;
    uint32_t valid_high;
    uint32_t data_low;
    uint32_t data_high;
    uint8_t secondaries;
    uint8_t name_length;
    bool directory;
    bool contiguous;
} EntrySet;

static bool read_stream(FatVolume *volume, FatFile *directory, uint32_t index, EntrySet *set)
{
    const uint8_t *stream = fat_read_entry(volume, directory, index);
    if (stream == NULL || stream[0] != ENTRY_STREAM) {
        return false;
    }
    set->contiguous = (stream[1] & FLAG_NO_FAT_CHAIN) != 0;
    set->name_length = stream[3];
    set->valid_low = fat_le32(stream + 8);
    set->valid_high = fat_le32(stream + 12);
    set->first_cluster = fat_le32(stream + 20);
    set->data_low = fat_le32(stream + 24);
    set->data_high = fat_le32(stream + 28);
    return true;
}

static void read_names(FatVolume *volume, FatFile *directory, uint32_t first, const EntrySet *set)
{
    uint32_t stored = 0;
    for (uint32_t secondary = 1; secondary < set->secondaries; secondary++) {
        const uint8_t *entry = fat_read_entry(volume, directory, first + secondary);
        if (entry == NULL || entry[0] != ENTRY_NAME) {
            continue;
        }
        for (uint32_t unit = 0; unit < NAME_UNITS_PER_ENTRY; unit++) {
            if (stored < FAT_LONG_NAME_CAPACITY) {
                volume->long_name[stored++] = fat_le16(entry + 2 + 2 * unit);
            }
        }
    }
}

static bool file_from_set(const EntrySet *set, FatFile *found)
{
    uint32_t high = set->directory ? set->data_high : set->valid_high;
    uint32_t size = set->directory ? set->data_low : set->valid_low;
    if (high != 0 || (set->directory && size == 0)) {
        return false;
    }
    *found = (FatFile){.first_cluster = set->first_cluster,
                       .size = size,
                       .directory = set->directory,
                       .contiguous = set->contiguous};
    return true;
}

static bool read_set(FatVolume *volume, FatFile *directory, uint32_t index, EntrySet *set)
{
    const uint8_t *entry = fat_read_entry(volume, directory, index);
    set->secondaries = entry[1];
    set->directory = (fat_le16(entry + 4) & ATTRIBUTE_DIRECTORY) != 0;
    return set->secondaries >= MINIMUM_SECONDARIES && set->secondaries <= MAXIMUM_SECONDARIES &&
           read_stream(volume, directory, index + 1, set);
}

bool exfat_find_entry(FatVolume *volume, FatFile *directory, const char *name, size_t length,
                      FatFile *found)
{
    uint32_t index = 0;
    while (index < MAXIMUM_DIRECTORY_ENTRIES) {
        const uint8_t *entry = fat_read_entry(volume, directory, index);
        if (entry == NULL || entry[0] == ENTRY_END) {
            return false;
        }
        EntrySet set = {.secondaries = 0};
        if (entry[0] != ENTRY_FILE || !read_set(volume, directory, index, &set)) {
            index++;
            continue;
        }
        read_names(volume, directory, index + 1, &set);
        if (fat_name_matches(volume->long_name, set.name_length, name, length)) {
            return file_from_set(&set, found);
        }
        index += 1u + set.secondaries;
    }
    return false;
}

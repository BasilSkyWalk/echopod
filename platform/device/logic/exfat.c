#include "exfat.h"

#include "fat_layout.h"
#include "fat_names.h"

enum {
    ENTRY_END = 0x00,
    ENTRY_FILE = 0x85,
    ENTRY_STREAM = 0xC0,
    MODIFIED_TIMESTAMP = 12,
    MODIFIED_UTC_OFFSET = 23,
    UTC_OFFSET_VALID = 0x80,
    UTC_OFFSET_NEGATIVE = 0x40,
    UTC_OFFSET_QUARTERS = 0x7F,
    UTC_OFFSET_RANGE = 0x80,
    SECONDS_PER_QUARTER_HOUR = 900,
    SECONDS_PER_DAY = 86400,
    DAYS_PER_ERA = 146097,
    YEARS_PER_ERA = 400,
    DAYS_BEFORE_UNIX_EPOCH = 719468,
    FIRST_FAT_YEAR = 1980,
    ENTRY_NAME = 0xC1,
    ATTRIBUTE_HIDDEN = 0x02,
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
    uint32_t modified;
    uint8_t utc_offset;
    bool directory;
    bool hidden;
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

static uint32_t days_from_civil(uint32_t year, uint32_t month, uint32_t day)
{
    uint32_t shifted = month <= 2 ? year - 1u : year;
    uint32_t era = shifted / YEARS_PER_ERA;
    uint32_t year_of_era = shifted - era * YEARS_PER_ERA;
    uint32_t day_of_year = (153u * ((month + 9u) % 12u) + 2u) / 5u + day - 1u;
    uint32_t day_of_era = year_of_era * 365u + year_of_era / 4u - year_of_era / 100u + day_of_year;
    return era * DAYS_PER_ERA + day_of_era - DAYS_BEFORE_UNIX_EPOCH;
}

static int32_t utc_offset_half_seconds(uint8_t utc_offset)
{
    if ((utc_offset & UTC_OFFSET_VALID) == 0) {
        return 0;
    }
    int32_t quarters = utc_offset & UTC_OFFSET_QUARTERS;
    if ((utc_offset & UTC_OFFSET_NEGATIVE) != 0) {
        quarters -= UTC_OFFSET_RANGE;
    }
    return quarters * (SECONDS_PER_QUARTER_HOUR / 2);
}

uint32_t fat_exfat_utc_stamp(uint32_t timestamp, uint8_t utc_offset)
{
    uint32_t date = timestamp >> 16;
    uint32_t days = days_from_civil(FIRST_FAT_YEAR + (date >> 9), date >> 5 & 15u, date & 31u);
    uint32_t clock =
        (timestamp >> 11 & 31u) * 1800u + (timestamp >> 5 & 63u) * 30u + (timestamp & 31u);
    uint32_t local = days * (SECONDS_PER_DAY / 2) + clock;
    return local - (uint32_t)utc_offset_half_seconds(utc_offset);
}

static bool file_from_set(const EntrySet *set, FatFile *found)
{
    uint32_t high = set->directory ? set->data_high : set->valid_high | set->data_high;
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
    set->hidden = (fat_le16(entry + 4) & ATTRIBUTE_HIDDEN) != 0;
    set->modified = fat_le32(entry + MODIFIED_TIMESTAMP);
    set->utc_offset = entry[MODIFIED_UTC_OFFSET];
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

bool exfat_next_entry(FatVolume *volume, FatDirCursor *cursor, FatDirEntry *found)
{
    while (cursor->index < MAXIMUM_DIRECTORY_ENTRIES) {
        uint32_t index = cursor->index;
        const uint8_t *entry = fat_read_entry(volume, &cursor->directory, index);
        if (entry == NULL || entry[0] == ENTRY_END) {
            return false;
        }
        EntrySet set = {.secondaries = 0};
        if (entry[0] != ENTRY_FILE || !read_set(volume, &cursor->directory, index, &set)) {
            cursor->index++;
            continue;
        }
        cursor->index += 1u + set.secondaries;
        read_names(volume, &cursor->directory, index + 1, &set);
        if (file_from_set(&set, &found->file)) {
            found->hidden = set.hidden;
            found->modified = fat_exfat_utc_stamp(set.modified, set.utc_offset);
            found->listed_size = set.data_low;
            fat_utf8_from_units(volume->long_name, set.name_length, found->name,
                                sizeof found->name);
            return true;
        }
    }
    return false;
}

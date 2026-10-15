#include "device/fat.h"

#include <stddef.h>

#include "exfat.h"
#include "fat_layout.h"
#include "fat_names.h"

enum {
    ENTRY_SIZE = 32,
    ENTRY_END = 0x00,
    ENTRY_DELETED = 0xE5,
    ATTRIBUTES = 11,
    ATTRIBUTE_VOLUME_ID = 0x08,
    ATTRIBUTE_DIRECTORY = 0x10,
    ATTRIBUTE_LONG_NAME = 0x0F,
    LONG_NAME_LAST = 0x40,
    LONG_NAME_ORDINAL_MASK = 0x1F,
    LONG_NAME_MAX_ENTRIES = 20,
    LONG_NAME_CHECKSUM = 13,
    CHARACTERS_PER_ENTRY = 13,
    SHORT_NAME_LENGTH = 11,
    SHORT_BASE_LENGTH = 8,
    KANJI_E5_MARKER = 0x05,
    MAXIMUM_DIRECTORY_ENTRIES = 65536
};

static const uint8_t LONG_NAME_OFFSETS[CHARACTERS_PER_ENTRY] = {1,  3,  5,  7,  9,  14, 16,
                                                                18, 20, 22, 24, 28, 30};

typedef struct LongName {
    bool valid;
    uint8_t checksum;
    uint8_t expected;
    uint8_t entries;
} LongName;

static void collect_long_name(FatVolume *volume, LongName *name, const uint8_t *entry)
{
    uint8_t ordinal = entry[0] & LONG_NAME_ORDINAL_MASK;
    if ((entry[0] & LONG_NAME_LAST) != 0) {
        *name = (LongName){ordinal >= 1 && ordinal <= LONG_NAME_MAX_ENTRIES,
                           entry[LONG_NAME_CHECKSUM], ordinal, ordinal};
    }
    if (!name->valid || ordinal != name->expected || entry[LONG_NAME_CHECKSUM] != name->checksum) {
        name->valid = false;
        return;
    }
    uint16_t *characters = volume->long_name + (ordinal - 1) * CHARACTERS_PER_ENTRY;
    for (int index = 0; index < CHARACTERS_PER_ENTRY; index++) {
        characters[index] = fat_le16(entry + LONG_NAME_OFFSETS[index]);
    }
    name->expected = (uint8_t)(ordinal - 1);
}

static uint8_t short_name_checksum(const uint8_t *entry)
{
    uint8_t sum = 0;
    for (int index = 0; index < SHORT_NAME_LENGTH; index++) {
        sum = (uint8_t)(((sum & 1) << 7) + (sum >> 1) + entry[index]);
    }
    return sum;
}

static bool long_name_applies(const LongName *name, const uint8_t *entry)
{
    return name->valid && name->expected == 0 && name->checksum == short_name_checksum(entry);
}

static bool matches_long_name(const FatVolume *volume, const LongName *long_name, const char *name,
                              size_t length)
{
    size_t limit = (size_t)long_name->entries * CHARACTERS_PER_ENTRY;
    return fat_name_matches(volume->long_name, limit, name, length);
}

static size_t trimmed_length(const uint8_t *text, size_t length)
{
    while (length > 0 && text[length - 1] == ' ') {
        length--;
    }
    return length;
}

static size_t short_name_text(const uint8_t *entry, char *text)
{
    size_t base = trimmed_length(entry, SHORT_BASE_LENGTH);
    size_t extension = trimmed_length(entry + SHORT_BASE_LENGTH, 3);
    size_t length = 0;
    for (size_t index = 0; index < base; index++) {
        text[length++] = (char)entry[index];
    }
    if (base > 0 && entry[0] == KANJI_E5_MARKER) {
        text[0] = (char)ENTRY_DELETED;
    }
    if (extension > 0) {
        text[length++] = '.';
    }
    for (size_t index = 0; index < extension; index++) {
        text[length++] = (char)entry[SHORT_BASE_LENGTH + index];
    }
    return length;
}

static bool matches_short_name(const uint8_t *entry, const char *name, size_t length)
{
    char text[SHORT_NAME_LENGTH + 1];
    size_t text_length = short_name_text(entry, text);
    if (text_length != length) {
        return false;
    }
    for (size_t index = 0; index < length; index++) {
        if (fat_fold((uint8_t)text[index]) != fat_fold((uint8_t)name[index])) {
            return false;
        }
    }
    return true;
}

static FatFile entry_file(const FatVolume *volume, const uint8_t *entry)
{
    uint32_t high = volume->type == FAT_TYPE_32 ? (uint32_t)fat_le16(entry + 20) << 16 : 0;
    bool directory = (entry[ATTRIBUTES] & ATTRIBUTE_DIRECTORY) != 0;
    return (FatFile){.first_cluster = high | fat_le16(entry + 26),
                     .size = directory ? UINT32_MAX : fat_le32(entry + 28),
                     .directory = directory};
}

static bool entry_matches(FatVolume *volume, const LongName *long_name, const uint8_t *entry,
                          const char *name, size_t length)
{
    if (long_name_applies(long_name, entry)) {
        return matches_long_name(volume, long_name, name, length);
    }
    return matches_short_name(entry, name, length);
}

static bool find_fat_entry(FatVolume *volume, FatFile *directory, const char *name, size_t length,
                           FatFile *found)
{
    LongName long_name = {.valid = false};
    for (uint32_t index = 0; index < MAXIMUM_DIRECTORY_ENTRIES; index++) {
        const uint8_t *entry = fat_read_entry(volume, directory, index);
        if (entry == NULL || entry[0] == ENTRY_END) {
            return false;
        }
        if (entry[0] == ENTRY_DELETED) {
            long_name.valid = false;
        } else if (entry[ATTRIBUTES] == ATTRIBUTE_LONG_NAME) {
            collect_long_name(volume, &long_name, entry);
        } else if ((entry[ATTRIBUTES] & ATTRIBUTE_VOLUME_ID) == 0 &&
                   entry_matches(volume, &long_name, entry, name, length)) {
            *found = entry_file(volume, entry);
            return true;
        } else {
            long_name.valid = false;
        }
    }
    return false;
}

static bool find_entry(FatVolume *volume, FatFile *directory, const char *name, size_t length,
                       FatFile *found)
{
    if (volume->type == FAT_TYPE_EXFAT) {
        return exfat_find_entry(volume, directory, name, length, found);
    }
    return find_fat_entry(volume, directory, name, length, found);
}

static size_t component_length(const char *path)
{
    size_t length = 0;
    while (path[length] != '\0' && path[length] != '/') {
        length++;
    }
    return length;
}

bool fat_open(FatVolume *volume, const char *path, FatFile *file)
{
    FatFile current = fat_root_directory(volume);
    bool found_any = false;
    while (*path != '\0') {
        if (*path == '/') {
            path++;
            continue;
        }
        size_t length = component_length(path);
        if (!current.directory || !find_entry(volume, &current, path, length, &current)) {
            return false;
        }
        found_any = true;
        path += length;
    }
    if (!found_any || current.directory) {
        return false;
    }
    current.cursor_index = 0;
    current.cursor_cluster = 0;
    *file = current;
    return true;
}

bool fat_read(FatVolume *volume, FatFile *file, uint32_t offset, void *buffer, uint32_t length)
{
    if (offset > file->size || length > file->size - offset) {
        return false;
    }
    uint8_t *out = buffer;
    uint32_t last_sector = (file->size - 1) / FAT_SECTOR_SIZE;
    while (length > 0) {
        uint32_t within = offset % FAT_SECTOR_SIZE;
        uint32_t chunk = FAT_SECTOR_SIZE - within < length ? FAT_SECTOR_SIZE - within : length;
        uint32_t sector_index = offset / FAT_SECTOR_SIZE;
        uint32_t lba = 0;
        if (!fat_file_sector(volume, file, sector_index, &lba)) {
            return false;
        }
        if (!fat_sector_cached(volume, lba)) {
            uint32_t ahead = last_sector - sector_index + 1;
            uint32_t run = fat_run_length(volume, file, sector_index,
                                          ahead < FAT_CACHE_SECTORS ? ahead : FAT_CACHE_SECTORS);
            if (run == 0 || !fat_read_run(volume, lba, run)) {
                return false;
            }
        }
        const uint8_t *source = fat_cached_sector(volume) + within;
        for (uint32_t index = 0; index < chunk; index++) {
            out[index] = source[index];
        }
        out += chunk;
        offset += chunk;
        length -= chunk;
    }
    return true;
}

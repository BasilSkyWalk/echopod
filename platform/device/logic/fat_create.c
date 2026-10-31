#include "fat_create.h"

#include "fat_layout.h"

#define FAT32_ENTRY_MASK 0x0FFFFFFFu
#define FAT32_END_OF_CHAIN 0x0FFFFFFFu
#define INFO_LEAD_SIGNATURE 0x41615252u
#define INFO_STRUCT_SIGNATURE 0x61417272u
#define INFO_UNKNOWN 0xFFFFFFFFu

enum {
    FIRST_CLUSTER = 2,
    FAT16_END_OF_CHAIN = 0xFFFF,
    ENTRY_SIZE = 32,
    ENTRIES_PER_SECTOR = FAT_SECTOR_SIZE / ENTRY_SIZE,
    ENTRY_END = 0x00,
    ENTRY_DELETED = 0xE5,
    ATTRIBUTES = 11,
    ATTRIBUTE_DIRECTORY = 0x10,
    ATTRIBUTE_ARCHIVE = 0x20,
    ATTRIBUTE_LONG_NAME = 0x0F,
    CASE_FLAGS = 12,
    LOWER_BASE = 0x08,
    LOWER_EXTENSION = 0x10,
    SHORT_NAME_LENGTH = 11,
    SHORT_BASE_LENGTH = 8,
    SHORT_EXTENSION_LENGTH = 3,
    TAIL_BASE_LIMIT = 6,
    TAIL_LIMIT = 99,
    LONG_NAME_LAST = 0x40,
    CHARACTERS_PER_LONG_ENTRY = 13,
    LONG_ENTRY_CHECKSUM = 13,
    MAXIMUM_LONG_ENTRIES = (FAT_RESERVE_NAME_LIMIT + CHARACTERS_PER_LONG_ENTRY - 1) /
        CHARACTERS_PER_LONG_ENTRY,
    MAXIMUM_DIRECTORY_ENTRIES = 65536,
    INFO_STRUCT_OFFSET = 484,
    INFO_FREE_OFFSET = 488,
    INFO_NEXT_OFFSET = 492,
    READ_AHEAD_SECTORS = 8
};

static const uint8_t LONG_NAME_OFFSETS[CHARACTERS_PER_LONG_ENTRY] = {1,  3,  5,  7,  9,  14, 16,
                                                                     18, 20, 22, 24, 28, 30};

typedef struct ShortName {
    uint8_t bytes[SHORT_NAME_LENGTH];
    uint8_t case_flags;
    bool needs_long_name;
} ShortName;

static void put16(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *bytes, uint32_t value)
{
    put16(bytes, value);
    put16(bytes + 2, value >> 16);
}

static uint32_t entries_per_fat_sector(const FatVolume *volume)
{
    return volume->type == FAT_TYPE_16 ? FAT_SECTOR_SIZE / 2 : FAT_SECTOR_SIZE / 4;
}

static uint32_t entry_at(const FatVolume *volume, const uint8_t *sector, uint32_t within)
{
    if (volume->type == FAT_TYPE_16) {
        return fat_le16(sector + within * 2);
    }
    return fat_le32(sector + within * 4) & FAT32_ENTRY_MASK;
}

static bool read_fat_sectors(FatVolume *volume, uint32_t sector_index)
{
    uint32_t left = volume->fat_sectors - sector_index;
    uint32_t count = left < READ_AHEAD_SECTORS ? left : READ_AHEAD_SECTORS;
    return fat_read_run(volume, volume->fat_start + sector_index, count);
}

static FatReserveResult scan_free_run(FatVolume *volume, uint32_t from, uint32_t needed,
                                      uint32_t *first)
{
    uint32_t per_sector = entries_per_fat_sector(volume);
    uint32_t end = volume->cluster_count + FIRST_CLUSTER;
    uint32_t run = 0;
    for (uint32_t cluster = from; cluster < end; cluster++) {
        if ((cluster == from || cluster % per_sector == 0) &&
            !read_fat_sectors(volume, cluster / per_sector)) {
            return FAT_RESERVE_FAILED;
        }
        run = entry_at(volume, fat_cached_sector(volume), cluster % per_sector) == 0 ? run + 1 : 0;
        if (run == needed) {
            *first = cluster + 1 - needed;
            return FAT_RESERVE_CREATED;
        }
    }
    return FAT_RESERVE_NO_SPACE;
}

static uint32_t info_next_free(FatVolume *volume)
{
    if (volume->info_sector == 0 || !fat_read_sector(volume, volume->info_sector)) {
        return 0;
    }
    const uint8_t *sector = fat_cached_sector(volume);
    bool valid = fat_le32(sector) == INFO_LEAD_SIGNATURE &&
                 fat_le32(sector + INFO_STRUCT_OFFSET) == INFO_STRUCT_SIGNATURE;
    return valid ? fat_le32(sector + INFO_NEXT_OFFSET) : 0;
}

static uint32_t search_start(FatVolume *volume)
{
    if (volume->free_hint == 0) {
        volume->free_hint = info_next_free(volume);
    }
    uint32_t end = volume->cluster_count + FIRST_CLUSTER;
    return volume->free_hint > FIRST_CLUSTER && volume->free_hint < end ? volume->free_hint
                                                                        : FIRST_CLUSTER;
}

static FatReserveResult find_free_run(FatVolume *volume, uint32_t needed, uint32_t *first)
{
    uint32_t from = search_start(volume);
    FatReserveResult result = scan_free_run(volume, from, needed, first);
    if (result == FAT_RESERVE_NO_SPACE && from != FIRST_CLUSTER) {
        result = scan_free_run(volume, FIRST_CLUSTER, needed, first);
    }
    if (result == FAT_RESERVE_CREATED) {
        volume->free_hint = *first + needed;
    }
    return result;
}

static uint32_t chain_value(const FatVolume *volume, uint32_t cluster, uint32_t last)
{
    if (cluster != last) {
        return cluster + 1;
    }
    return volume->type == FAT_TYPE_16 ? FAT16_END_OF_CHAIN : FAT32_END_OF_CHAIN;
}

static void set_entry(const FatVolume *volume, uint8_t *sector, uint32_t within, uint32_t value)
{
    if (volume->type == FAT_TYPE_16) {
        put16(sector + within * 2, value);
        return;
    }
    uint32_t kept = fat_le32(sector + within * 4) & ~FAT32_ENTRY_MASK;
    put32(sector + within * 4, kept | value);
}

static bool write_chain_copy(FatVolume *volume, uint32_t copy_start, uint32_t first, uint32_t count)
{
    uint32_t per_sector = entries_per_fat_sector(volume);
    uint32_t last = first + count - 1;
    uint32_t cluster = first;
    while (cluster <= last) {
        uint32_t lba = copy_start + cluster / per_sector;
        uint8_t *sector = fat_sector_for_update(volume, lba);
        if (sector == NULL) {
            return false;
        }
        do {
            set_entry(volume, sector, cluster % per_sector, chain_value(volume, cluster, last));
            cluster++;
        } while (cluster <= last && cluster % per_sector != 0);
        if (!fat_commit_sector(volume, lba)) {
            return false;
        }
    }
    return true;
}

static bool write_chain(FatVolume *volume, uint32_t first, uint32_t count)
{
    for (uint32_t copy = 0; copy < volume->fat_count; copy++) {
        if (!write_chain_copy(volume, volume->fat_start + copy * volume->fat_sectors, first,
                              count)) {
            return false;
        }
    }
    volume->fat_sector_valid = false;
    return true;
}

static bool update_info(FatVolume *volume, uint32_t first, uint32_t count)
{
    if (volume->info_sector == 0) {
        return true;
    }
    uint8_t *sector = fat_sector_for_update(volume, volume->info_sector);
    if (sector == NULL) {
        return false;
    }
    if (fat_le32(sector) != INFO_LEAD_SIGNATURE ||
        fat_le32(sector + INFO_STRUCT_OFFSET) != INFO_STRUCT_SIGNATURE) {
        return true;
    }
    uint32_t free_count = fat_le32(sector + INFO_FREE_OFFSET);
    if (free_count != INFO_UNKNOWN) {
        put32(sector + INFO_FREE_OFFSET, free_count >= count ? free_count - count : 0);
    }
    put32(sector + INFO_NEXT_OFFSET, first + count);
    return fat_commit_sector(volume, volume->info_sector);
}

static bool short_name_character(char character)
{
    static const char SPECIAL[] = "!#$%&'()-@^_`{}~";
    if ((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
        (character >= '0' && character <= '9')) {
        return true;
    }
    for (const char *special = SPECIAL; *special != '\0'; special++) {
        if (character == *special) {
            return true;
        }
    }
    return false;
}

static size_t last_dot(const char *name, size_t length)
{
    for (size_t index = length; index > 0; index--) {
        if (name[index - 1] == '.') {
            return index - 1;
        }
    }
    return length;
}

static bool copy_part(const char *text, size_t length, uint8_t *out, size_t capacity,
                      uint8_t *case_flag)
{
    bool lower = false;
    bool upper = false;
    if (length > capacity) {
        return false;
    }
    for (size_t index = 0; index < length; index++) {
        if (!short_name_character(text[index])) {
            return false;
        }
        lower |= text[index] >= 'a' && text[index] <= 'z';
        upper |= text[index] >= 'A' && text[index] <= 'Z';
        out[index] = fat_upper_ascii(text[index]);
    }
    *case_flag = lower ? *case_flag : 0;
    return !(lower && upper);
}

static void blank_short_name(ShortName *name)
{
    for (int index = 0; index < SHORT_NAME_LENGTH; index++) {
        name->bytes[index] = ' ';
    }
    name->case_flags = 0;
    name->needs_long_name = false;
}

static bool exact_short_name(const char *name, size_t length, ShortName *out)
{
    blank_short_name(out);
    size_t dot = last_dot(name, length);
    if (dot == 0 || last_dot(name, dot) != dot) {
        return false;
    }
    uint8_t base_flag = LOWER_BASE;
    uint8_t extension_flag = LOWER_EXTENSION;
    size_t extension_length = dot < length ? length - dot - 1 : 0;
    if (!copy_part(name, dot, out->bytes, SHORT_BASE_LENGTH, &base_flag) ||
        !copy_part(name + length - extension_length, extension_length,
                   out->bytes + SHORT_BASE_LENGTH, SHORT_EXTENSION_LENGTH, &extension_flag)) {
        return false;
    }
    out->case_flags = (uint8_t)(base_flag | extension_flag);
    return true;
}

static uint8_t basis_character(char character)
{
    return short_name_character(character) ? fat_upper_ascii(character) : '_';
}

static size_t basis_name(const char *name, size_t length, ShortName *out)
{
    blank_short_name(out);
    out->needs_long_name = true;
    size_t start = 0;
    while (start < length && name[start] == '.') {
        start++;
    }
    size_t dot = last_dot(name, length);
    size_t base_end = dot > start ? dot : length;
    size_t base_length = 0;
    for (size_t index = start; index < base_end && base_length < SHORT_BASE_LENGTH; index++) {
        if (name[index] != ' ' && name[index] != '.') {
            out->bytes[base_length++] = basis_character(name[index]);
        }
    }
    size_t extension_length = 0;
    for (size_t index = base_end + 1; index < length && extension_length < 3; index++) {
        if (name[index] != ' ') {
            out->bytes[SHORT_BASE_LENGTH + extension_length++] = basis_character(name[index]);
        }
    }
    if (base_length == 0) {
        out->bytes[base_length++] = '_';
    }
    return base_length;
}

static void apply_tail(ShortName *name, size_t base_length, uint32_t number)
{
    uint8_t digits[2];
    size_t digit_count = number < 10 ? 1 : 2;
    digits[0] = (uint8_t)('0' + (digit_count == 2 ? number / 10 : number));
    digits[1] = (uint8_t)('0' + number % 10);
    size_t kept = base_length < TAIL_BASE_LIMIT ? base_length : TAIL_BASE_LIMIT;
    kept = kept + 1 + digit_count <= SHORT_BASE_LENGTH ? kept : SHORT_BASE_LENGTH - 1 - digit_count;
    size_t position = kept;
    name->bytes[position++] = '~';
    for (size_t index = 0; index < digit_count; index++) {
        name->bytes[position++] = digits[index];
    }
    while (position < SHORT_BASE_LENGTH) {
        name->bytes[position++] = ' ';
    }
}

static bool same_short_name(const uint8_t *entry, const ShortName *name)
{
    for (int index = 0; index < SHORT_NAME_LENGTH; index++) {
        if (entry[index] != name->bytes[index]) {
            return false;
        }
    }
    return true;
}

static FatReserveResult short_name_taken(FatVolume *volume, FatFile *parent, const ShortName *name)
{
    for (uint32_t index = 0; index < MAXIMUM_DIRECTORY_ENTRIES; index++) {
        const uint8_t *entry = fat_read_entry(volume, parent, index);
        if (entry == NULL || entry[0] == ENTRY_END) {
            return FAT_RESERVE_CREATED;
        }
        if (entry[0] != ENTRY_DELETED && entry[ATTRIBUTES] != ATTRIBUTE_LONG_NAME &&
            same_short_name(entry, name)) {
            return FAT_RESERVE_EXISTS;
        }
    }
    return FAT_RESERVE_CREATED;
}

static FatReserveResult choose_short_name(FatVolume *volume, FatFile *parent,
                                          const FatNewEntry *entry, ShortName *name)
{
    if (exact_short_name(entry->name, entry->length, name)) {
        bool taken = short_name_taken(volume, parent, name) == FAT_RESERVE_EXISTS;
        return taken ? FAT_RESERVE_FAILED : FAT_RESERVE_CREATED;
    }
    size_t base_length = basis_name(entry->name, entry->length, name);
    for (uint32_t number = 1; number <= TAIL_LIMIT; number++) {
        apply_tail(name, base_length, number);
        if (short_name_taken(volume, parent, name) == FAT_RESERVE_CREATED) {
            return FAT_RESERVE_CREATED;
        }
    }
    return FAT_RESERVE_FAILED;
}

static uint32_t long_entry_count(const FatNewEntry *entry, const ShortName *name)
{
    if (!name->needs_long_name) {
        return 0;
    }
    return ((uint32_t)entry->length + CHARACTERS_PER_LONG_ENTRY - 1) / CHARACTERS_PER_LONG_ENTRY;
}

static FatReserveResult find_free_entries(FatVolume *volume, FatFile *parent, uint32_t needed,
                                          uint32_t *first)
{
    uint32_t run = 0;
    for (uint32_t index = 0; index < MAXIMUM_DIRECTORY_ENTRIES; index++) {
        const uint8_t *entry = fat_read_entry(volume, parent, index);
        if (entry == NULL) {
            return FAT_RESERVE_FAILED;
        }
        run = entry[0] == ENTRY_END || entry[0] == ENTRY_DELETED ? run + 1 : 0;
        if (run == needed) {
            *first = index + 1 - needed;
            return FAT_RESERVE_CREATED;
        }
    }
    return FAT_RESERVE_FAILED;
}

static uint8_t short_name_checksum(const uint8_t *name)
{
    uint8_t sum = 0;
    for (int index = 0; index < SHORT_NAME_LENGTH; index++) {
        sum = (uint8_t)(((sum & 1) << 7) + (sum >> 1) + name[index]);
    }
    return sum;
}

static uint16_t new_date(void)
{
    return (uint16_t)((FAT_NEW_DATE_YEAR - 1980) << 9 | FAT_NEW_DATE_MONTH << 5 | FAT_NEW_DATE_DAY);
}

static void clear_entry(uint8_t *entry)
{
    for (int index = 0; index < ENTRY_SIZE; index++) {
        entry[index] = 0;
    }
}

static void fill_long_entry(uint8_t *out, const FatNewEntry *entry, uint32_t ordinal,
                            uint8_t checksum)
{
    clear_entry(out);
    uint32_t count =
        ((uint32_t)entry->length + CHARACTERS_PER_LONG_ENTRY - 1) / CHARACTERS_PER_LONG_ENTRY;
    out[0] = (uint8_t)(ordinal | (ordinal == count ? LONG_NAME_LAST : 0));
    out[ATTRIBUTES] = ATTRIBUTE_LONG_NAME;
    out[LONG_ENTRY_CHECKSUM] = checksum;
    for (uint32_t slot = 0; slot < CHARACTERS_PER_LONG_ENTRY; slot++) {
        size_t position = (ordinal - 1) * CHARACTERS_PER_LONG_ENTRY + slot;
        uint32_t unit = position < entry->length    ? (uint8_t)entry->name[position]
                        : position == entry->length ? 0x0000
                                                    : 0xFFFF;
        put16(out + LONG_NAME_OFFSETS[slot], unit);
    }
}

static void fill_short_entry(uint8_t *out, const uint8_t *name, uint8_t case_flags,
                             const FatFile *file)
{
    clear_entry(out);
    for (int index = 0; index < SHORT_NAME_LENGTH; index++) {
        out[index] = name[index];
    }
    out[ATTRIBUTES] = file->directory ? ATTRIBUTE_DIRECTORY : ATTRIBUTE_ARCHIVE;
    out[CASE_FLAGS] = case_flags;
    put16(out + 16, new_date());
    put16(out + 18, new_date());
    put16(out + 20, file->first_cluster >> 16);
    put16(out + 24, new_date());
    put16(out + 26, file->first_cluster);
    put32(out + 28, file->directory ? 0 : file->size);
}

static bool write_entry(FatVolume *volume, FatFile *parent, uint32_t index, const uint8_t *entry)
{
    uint32_t lba = 0;
    uint32_t offset = index * ENTRY_SIZE;
    return fat_file_sector(volume, parent, offset / FAT_SECTOR_SIZE, &lba) &&
           fat_update_bytes(volume, lba, offset % FAT_SECTOR_SIZE, entry, ENTRY_SIZE);
}

static bool write_entries(FatVolume *volume, FatFile *parent, uint32_t first,
                          const FatNewEntry *entry, const ShortName *name, const FatFile *file)
{
    uint32_t long_entries = long_entry_count(entry, name);
    uint8_t checksum = short_name_checksum(name->bytes);
    uint8_t bytes[ENTRY_SIZE];
    for (uint32_t index = 0; index < long_entries; index++) {
        fill_long_entry(bytes, entry, long_entries - index, checksum);
        if (!write_entry(volume, parent, first + index, bytes)) {
            return false;
        }
    }
    fill_short_entry(bytes, name->bytes, name->case_flags, file);
    return write_entry(volume, parent, first + long_entries, bytes);
}

static uint32_t parent_cluster(const FatVolume *volume, const FatFile *parent)
{
    bool root = parent->first_cluster == 0 ||
                (volume->type == FAT_TYPE_32 && parent->first_cluster == volume->root_cluster);
    return root ? 0 : parent->first_cluster;
}

static bool write_dot_entries(FatVolume *volume, const FatFile *created, uint32_t parent)
{
    static const uint8_t DOT[SHORT_NAME_LENGTH] = {'.', ' ', ' ', ' ', ' ', ' ',
                                                   ' ', ' ', ' ', ' ', ' '};
    static const uint8_t DOT_DOT[SHORT_NAME_LENGTH] = {'.', '.', ' ', ' ', ' ', ' ',
                                                       ' ', ' ', ' ', ' ', ' '};
    uint8_t bytes[2 * ENTRY_SIZE];
    FatFile up = {.first_cluster = parent, .directory = true};
    fill_short_entry(bytes, DOT, 0, created);
    fill_short_entry(bytes + ENTRY_SIZE, DOT_DOT, 0, &up);
    return fat_update_bytes(volume, fat_cluster_lba(volume, created->first_cluster), 0, bytes,
                            sizeof bytes);
}

static bool write_contents(FatVolume *volume, const FatNewEntry *entry, const FatFile *created,
                           const FatFile *parent)
{
    uint32_t lba = fat_cluster_lba(volume, created->first_cluster);
    if (!fat_clear_sectors(volume, lba, fat_new_cleared_sectors(volume, entry))) {
        return false;
    }
    return !entry->directory || write_dot_entries(volume, created, parent_cluster(volume, parent));
}

FatReserveResult fat_create_entry(FatVolume *volume, FatFile *parent, const FatNewEntry *entry,
                                  FatFile *created)
{
    ShortName name;
    uint32_t slot = 0;
    uint32_t first = 0;
    uint32_t clusters = fat_new_cluster_count(volume, entry);
    FatReserveResult result = choose_short_name(volume, parent, entry, &name);
    if (result == FAT_RESERVE_CREATED) {
        result = find_free_entries(volume, parent, long_entry_count(entry, &name) + 1, &slot);
    }
    if (result == FAT_RESERVE_CREATED) {
        result = find_free_run(volume, clusters, &first);
    }
    if (result != FAT_RESERVE_CREATED) {
        return result;
    }
    *created = (FatFile){.first_cluster = first,
                         .size = entry->directory ? UINT32_MAX : entry->size,
                         .directory = entry->directory};
    bool written = write_chain(volume, first, clusters) && update_info(volume, first, clusters) &&
                   write_contents(volume, entry, created, parent) &&
                   write_entries(volume, parent, slot, entry, &name, created);
    return written ? FAT_RESERVE_CREATED : FAT_RESERVE_FAILED;
}

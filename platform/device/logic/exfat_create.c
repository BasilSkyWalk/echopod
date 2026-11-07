#include "fat_create.h"

#include "fat_layout.h"

enum {
    FIRST_CLUSTER = 2,
    ENTRY_SIZE = 32,
    ENTRY_END = 0x00,
    ENTRY_IN_USE = 0x80,
    ENTRY_BITMAP = 0x81,
    ENTRY_FILE = 0x85,
    ENTRY_STREAM = 0xC0,
    ENTRY_NAME = 0xC1,
    ATTRIBUTE_DIRECTORY = 0x10,
    ATTRIBUTE_ARCHIVE = 0x20,
    STREAM_FLAGS = 0x03,
    FLAG_ALLOCATION_POSSIBLE = 0x01,
    FLAG_NO_FAT_CHAIN = 0x02,
    MAXIMUM_GROW_SET = 19,
    NAME_UNITS_PER_ENTRY = 15,
    MAXIMUM_SET_ENTRIES = 2 +
        (FAT_RESERVE_NAME_LIMIT + NAME_UNITS_PER_ENTRY - 1) / NAME_UNITS_PER_ENTRY,
    MAXIMUM_DIRECTORY_ENTRIES = 0x100000,
    BITS_PER_BYTE = 8,
    BITS_PER_SECTOR = FAT_SECTOR_SIZE * BITS_PER_BYTE
};

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

static bool find_bitmap(FatVolume *volume, FatFile *bitmap)
{
    FatFile root = fat_root_directory(volume);
    for (uint32_t index = 0; index < MAXIMUM_DIRECTORY_ENTRIES; index++) {
        const uint8_t *entry = fat_read_entry(volume, &root, index);
        if (entry == NULL || entry[0] == ENTRY_END) {
            return false;
        }
        if (entry[0] == ENTRY_BITMAP && (entry[1] & 1) == 0) {
            *bitmap =
                (FatFile){.first_cluster = fat_le32(entry + 20), .size = fat_le32(entry + 24)};
            return bitmap->size >= (volume->cluster_count + BITS_PER_BYTE - 1) / BITS_PER_BYTE;
        }
    }
    return false;
}

static bool bitmap_sector(FatVolume *volume, FatFile *bitmap, uint32_t sector_index)
{
    uint32_t lba = 0;
    if (!fat_file_sector(volume, bitmap, sector_index, &lba)) {
        return false;
    }
    uint32_t run = fat_sector_cached(volume, lba)
                       ? 1
                       : fat_run_length(volume, bitmap, sector_index, FAT_CACHE_SECTORS);
    return run != 0 && fat_read_run(volume, lba, run);
}

static FatReserveResult find_free_run(FatVolume *volume, FatFile *bitmap, uint32_t needed,
                                      uint32_t *first)
{
    uint32_t run = 0;
    for (uint32_t bit = 0; bit < volume->cluster_count; bit++) {
        if (bit % BITS_PER_SECTOR == 0 && !bitmap_sector(volume, bitmap, bit / BITS_PER_SECTOR)) {
            return FAT_RESERVE_FAILED;
        }
        uint32_t within = bit % BITS_PER_SECTOR;
        uint8_t byte = fat_cached_sector(volume)[within / BITS_PER_BYTE];
        run = (byte >> (within % BITS_PER_BYTE) & 1u) == 0 ? run + 1 : 0;
        if (run == needed) {
            *first = bit + 1 - needed + FIRST_CLUSTER;
            return FAT_RESERVE_CREATED;
        }
    }
    return FAT_RESERVE_NO_SPACE;
}

static bool mark_allocated(FatVolume *volume, FatFile *bitmap, uint32_t first, uint32_t count)
{
    uint32_t bit = first - FIRST_CLUSTER;
    uint32_t end = bit + count;
    while (bit < end) {
        uint32_t lba = 0;
        if (!fat_file_sector(volume, bitmap, bit / BITS_PER_SECTOR, &lba)) {
            return false;
        }
        uint8_t *sector = fat_sector_for_update(volume, lba);
        if (sector == NULL) {
            return false;
        }
        do {
            uint32_t within = bit % BITS_PER_SECTOR;
            sector[within / BITS_PER_BYTE] |= (uint8_t)(1u << (within % BITS_PER_BYTE));
            bit++;
        } while (bit < end && bit % BITS_PER_SECTOR != 0);
        if (!fat_commit_sector(volume, lba)) {
            return false;
        }
    }
    return true;
}

static uint32_t set_entry_count(const FatNewEntry *entry)
{
    return 2u + ((uint32_t)entry->length + NAME_UNITS_PER_ENTRY - 1) / NAME_UNITS_PER_ENTRY;
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
        run = (entry[0] & ENTRY_IN_USE) == 0 ? run + 1 : 0;
        if (run == needed) {
            *first = index + 1 - needed;
            return FAT_RESERVE_CREATED;
        }
    }
    return FAT_RESERVE_FAILED;
}

static uint16_t rotate_add(uint16_t value, uint8_t byte)
{
    return (uint16_t)(((value & 1u) != 0 ? 0x8000u : 0u) + (value >> 1) + byte);
}

static uint16_t name_hash(const FatNewEntry *entry)
{
    uint16_t hash = 0;
    for (size_t index = 0; index < entry->length; index++) {
        hash = rotate_add(hash, fat_upper_ascii(entry->name[index]));
        hash = rotate_add(hash, 0);
    }
    return hash;
}

static uint16_t set_checksum(const uint8_t *entries, uint32_t count)
{
    uint16_t checksum = 0;
    for (uint32_t index = 0; index < count * ENTRY_SIZE; index++) {
        if (index != 2 && index != 3) {
            checksum = rotate_add(checksum, entries[index]);
        }
    }
    return checksum;
}

static uint32_t new_timestamp(void)
{
    return fat_new_stamp();
}

static void fill_file_entry(uint8_t *entry, uint32_t count, bool directory)
{
    entry[0] = ENTRY_FILE;
    entry[1] = (uint8_t)(count - 1);
    put16(entry + 4, directory ? ATTRIBUTE_DIRECTORY : ATTRIBUTE_ARCHIVE);
    put32(entry + 8, new_timestamp());
    put32(entry + 12, new_timestamp());
    put32(entry + 16, new_timestamp());
}

static void fill_stream_entry(uint8_t *entry, const FatNewEntry *new_entry, const FatFile *file,
                              uint32_t length)
{
    entry[0] = ENTRY_STREAM;
    entry[1] = STREAM_FLAGS;
    entry[3] = (uint8_t)new_entry->length;
    put16(entry + 4, name_hash(new_entry));
    put32(entry + 8, length);
    put32(entry + 20, file->first_cluster);
    put32(entry + 24, length);
}

static void fill_name_entries(uint8_t *entries, const FatNewEntry *entry, uint32_t count)
{
    for (uint32_t slot = 0; slot < count; slot++) {
        uint8_t *name = entries + slot * ENTRY_SIZE;
        name[0] = ENTRY_NAME;
        for (uint32_t unit = 0; unit < NAME_UNITS_PER_ENTRY; unit++) {
            size_t position = slot * NAME_UNITS_PER_ENTRY + unit;
            put16(name + 2 + unit * 2,
                  position < entry->length ? (uint8_t)entry->name[position] : 0);
        }
    }
}

static void build_set(uint8_t *entries, const FatNewEntry *entry, const FatFile *file,
                      uint32_t length)
{
    uint32_t count = set_entry_count(entry);
    for (uint32_t index = 0; index < count * ENTRY_SIZE; index++) {
        entries[index] = 0;
    }
    fill_file_entry(entries, count, entry->directory);
    fill_stream_entry(entries + ENTRY_SIZE, entry, file, length);
    fill_name_entries(entries + 2 * ENTRY_SIZE, entry, count - 2);
    put16(entries + 2, set_checksum(entries, count));
}

static bool write_entry(FatVolume *volume, FatFile *parent, uint32_t index, const uint8_t *entry)
{
    uint32_t lba = 0;
    uint32_t offset = index * ENTRY_SIZE;
    return fat_file_sector(volume, parent, offset / FAT_SECTOR_SIZE, &lba) &&
           fat_update_bytes(volume, lba, offset % FAT_SECTOR_SIZE, entry, ENTRY_SIZE);
}

static bool write_set(FatVolume *volume, FatFile *parent, uint32_t first, const uint8_t *entries,
                      uint32_t count)
{
    for (uint32_t index = count; index > 0; index--) {
        if (!write_entry(volume, parent, first + index - 1, entries + (index - 1) * ENTRY_SIZE)) {
            return false;
        }
    }
    return true;
}

static uint32_t stored_length(const FatVolume *volume, const FatNewEntry *entry)
{
    return entry->directory ? (uint32_t)FAT_SECTOR_SIZE << volume->cluster_shift : entry->size;
}

FatReserveResult exfat_create_entry(FatVolume *volume, FatFile *parent, const FatNewEntry *entry,
                                    FatFile *created)
{
    FatFile bitmap;
    uint32_t slot = 0;
    uint32_t first = 0;
    uint32_t clusters = fat_new_cluster_count(volume, entry);
    uint32_t count = set_entry_count(entry);
    if (!find_bitmap(volume, &bitmap)) {
        return FAT_RESERVE_FAILED;
    }
    FatReserveResult result = find_free_entries(volume, parent, count, &slot);
    if (result == FAT_RESERVE_CREATED) {
        result = find_free_run(volume, &bitmap, clusters, &first);
    }
    if (result != FAT_RESERVE_CREATED) {
        return result;
    }
    uint32_t length = stored_length(volume, entry);
    *created = (FatFile){
        .first_cluster = first, .size = length, .directory = entry->directory, .contiguous = true};
    uint8_t entries[MAXIMUM_SET_ENTRIES * ENTRY_SIZE];
    build_set(entries, entry, created, length);
    uint32_t lba = fat_cluster_lba(volume, first);
    bool written = mark_allocated(volume, &bitmap, first, clusters) &&
                   fat_clear_sectors(volume, lba, fat_new_cleared_sectors(volume, entry)) &&
                   write_set(volume, parent, slot, entries, count);
    return written ? FAT_RESERVE_CREATED : FAT_RESERVE_FAILED;
}

static bool bitmap_free(FatVolume *volume, void *context, uint32_t cluster, bool *free)
{
    uint32_t bit = cluster - FIRST_CLUSTER;
    if (!bitmap_sector(volume, context, bit / BITS_PER_SECTOR)) {
        return false;
    }
    uint32_t within = bit % BITS_PER_SECTOR;
    *free =
        (fat_cached_sector(volume)[within / BITS_PER_BYTE] >> (within % BITS_PER_BYTE) & 1u) == 0;
    return true;
}

static bool claim_bits(FatVolume *volume, void *context, uint32_t first, uint32_t count)
{
    return mark_allocated(volume, context, first, count);
}

static bool run_free(FatVolume *volume, FatFile *bitmap, uint32_t first, uint32_t count)
{
    if (first + count > volume->cluster_count + FIRST_CLUSTER || first + count < first) {
        return false;
    }
    for (uint32_t cluster = first; cluster < first + count; cluster++) {
        bool free = false;
        if (!bitmap_free(volume, bitmap, cluster, &free) || !free) {
            return false;
        }
    }
    return true;
}

static bool read_set_entries(FatVolume *volume, FatFile *parent, uint32_t index, uint8_t *entries,
                             uint32_t *count)
{
    const uint8_t *file = fat_read_entry(volume, parent, index);
    if (file == NULL || file[0] != ENTRY_FILE || file[1] < 1 || file[1] >= MAXIMUM_GROW_SET) {
        return false;
    }
    *count = 1u + file[1];
    for (uint32_t entry = 0; entry < *count; entry++) {
        const uint8_t *bytes = fat_read_entry(volume, parent, index + entry);
        if (bytes == NULL) {
            return false;
        }
        for (uint32_t offset = 0; offset < ENTRY_SIZE; offset++) {
            entries[entry * ENTRY_SIZE + offset] = bytes[offset];
        }
    }
    return entries[ENTRY_SIZE] == ENTRY_STREAM;
}

static FatReserveResult extend(FatVolume *volume, FatFile *bitmap, FatFile file, uint8_t *stream,
                               uint32_t size)
{
    bool contiguous = (stream[1] & FLAG_NO_FAT_CHAIN) != 0;
    uint32_t have = fat_clusters_for(volume, fat_le32(stream + 24));
    uint32_t needed = fat_clusters_for(volume, size) - have;
    file.contiguous = contiguous;
    if (have == 0 && fat_le32(stream + 20) == 0) {
        uint32_t first = 0;
        FatReserveResult found = find_free_run(volume, bitmap, needed, &first);
        stream[1] |= FLAG_ALLOCATION_POSSIBLE | FLAG_NO_FAT_CHAIN;
        put32(stream + 20, first);
        return found != FAT_RESERVE_CREATED                    ? found
               : mark_allocated(volume, bitmap, first, needed) ? FAT_RESERVE_CREATED
                                                               : FAT_RESERVE_FAILED;
    }
    uint32_t last =
        contiguous ? file.first_cluster + have - 1 : fat_last_cluster(volume, file, have);
    if (have == 0 || last < FIRST_CLUSTER) {
        return FAT_RESERVE_FAILED;
    }
    if (needed == 0) {
        return FAT_RESERVE_CREATED;
    }
    if (contiguous && run_free(volume, bitmap, last + 1, needed)) {
        return mark_allocated(volume, bitmap, last + 1, needed) ? FAT_RESERVE_CREATED
                                                                : FAT_RESERVE_FAILED;
    }
    if (contiguous && !fat_write_chain(volume, file.first_cluster, have)) {
        return FAT_RESERVE_FAILED;
    }
    FatGrowth growth = {bitmap_free, claim_bits, bitmap, last + 1, needed, last};
    uint32_t final = 0;
    stream[1] &= (uint8_t)~FLAG_NO_FAT_CHAIN;
    return fat_grow_chain(volume, &growth, &final);
}

FatReserveResult exfat_grow_entry(FatVolume *volume, FatFile *parent, uint32_t index, FatFile file,
                                  uint32_t size)
{
    FatFile bitmap;
    uint8_t entries[MAXIMUM_GROW_SET * ENTRY_SIZE];
    uint32_t count = 0;
    if (!find_bitmap(volume, &bitmap) ||
        !read_set_entries(volume, parent, index, entries, &count)) {
        return FAT_RESERVE_FAILED;
    }
    uint8_t *stream = entries + ENTRY_SIZE;
    if (fat_le32(stream + 28) != 0) {
        return FAT_RESERVE_FAILED;
    }
    FatReserveResult result = extend(volume, &bitmap, file, stream, size);
    if (result != FAT_RESERVE_CREATED) {
        return result;
    }
    put32(stream + 8, size);
    put32(stream + 12, 0);
    put32(stream + 24, size);
    put16(entries + 2, set_checksum(entries, count));
    bool written = write_entry(volume, parent, index + 1, stream) &&
                   write_entry(volume, parent, index, entries);
    return written ? FAT_RESERVE_CREATED : FAT_RESERVE_FAILED;
}

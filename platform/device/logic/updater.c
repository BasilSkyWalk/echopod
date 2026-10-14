#include "device/updater.h"

#include <stddef.h>

#include "device/image_crc.h"

enum {
    SECTOR_BYTES = 512,
    CHUNK_SECTORS = UPDATER_CHUNK_BYTES / SECTOR_BYTES,
    CHUNK_WORDS = UPDATER_CHUNK_BYTES / 4,
    HEADER_BYTES = 0x200,
    PREFIX_BYTES = 0x7B8,
    CONTAINER_BASE_OFFSET = 0x200,
    CONTAINER_CAPACITY_OFFSET = 0x204,
    TABLE_OFFSET = 0x208,
    TABLE_ROWS = 91,
    ROW_BYTES = 16,
    CODE_OFFSET = 0xD68,
    VECTOR_BYTES = 8,
    MINIMUM_DATA = 8,
    MAXIMUM_DATA = 180,
    FW_SIZE_OFFSET = 0x1F4,
    MAGIC_OFFSET = 0x1F8,
    FW1_END_OFFSET = 0x80,
    FW_SIZE = 0x44C000,
    FW_END = 0x500000,
    CHUNK_COUNT = FW_END / UPDATER_CHUNK_BYTES,
    PASSES = 2
};

#define CODE_BASE 0x03050000u
#define CODE_TARGET 0x03050B68u
#define CODE_LIMIT 0x030771D4u
#define DATA_BASE 0x03004000u

static const uint8_t MAGIC[8] = {'R', 'K', 'n', 'a', 'n', 'o', 'F', 'W'};
static const uint32_t ROW_2[4] = {0x03078CD0u, 0x03079DCCu, 4, 0};

static uint32_t source_chunk[CHUNK_WORDS];
static uint32_t target_chunk[CHUNK_WORDS];

typedef struct SlotCopy {
    const UpdateFile *file;
    const UpdateStorage *storage;
    uint32_t source_slot;
    uint32_t target_slot;
    UpdateProgress progress;
    uint32_t pass;
} SlotCopy;

static uint32_t le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static bool same_bytes(const uint32_t *left, const uint32_t *right, uint32_t words)
{
    for (uint32_t index = 0; index < words; index++) {
        if (left[index] != right[index]) {
            return false;
        }
    }
    return true;
}

static bool read_file_chunk(const UpdateFile *file, uint32_t chunk, uint32_t *words)
{
    return file->read(file->context, chunk * UPDATER_CHUNK_BYTES, words, UPDATER_CHUNK_BYTES);
}

static bool read_slot_chunk(const UpdateStorage *storage, uint32_t slot, uint32_t chunk,
                            uint32_t *words)
{
    return storage->read(storage->context, slot + chunk * CHUNK_SECTORS, CHUNK_SECTORS, words);
}

static bool header_is_valid(const uint8_t *header, const UpdateStorage *storage)
{
    bool magic = true;
    for (size_t index = 0; index < sizeof MAGIC; index++) {
        magic = magic && header[MAGIC_OFFSET + index] == MAGIC[index];
    }
    return magic && le32(header + FW_SIZE_OFFSET) == FW_SIZE &&
           le32(header + FW1_END_OFFSET) <= FW_SIZE &&
           FW_END / SECTOR_BYTES <= storage->slot_sectors &&
           le32(header + CONTAINER_BASE_OFFSET) == CODE_BASE &&
           le32(header + CONTAINER_CAPACITY_OFFSET) == TABLE_ROWS;
}

static uint32_t row_word(const uint8_t *header, uint32_t row, uint32_t word)
{
    return le32(header + TABLE_OFFSET + ROW_BYTES * row + 4 * word);
}

static bool code_row_is_valid(const uint8_t *header, uint32_t *code_size)
{
    uint32_t size = row_word(header, 0, 2);
    *code_size = size;
    return row_word(header, 0, 0) == CODE_TARGET && row_word(header, 0, 1) == CODE_BASE &&
           row_word(header, 0, 3) == CODE_TARGET + size && size % 4 == 0 && size >= VECTOR_BYTES &&
           size <= CODE_LIMIT - CODE_TARGET;
}

static bool data_row_is_valid(const uint8_t *header)
{
    uint32_t size = row_word(header, 1, 1);
    return row_word(header, 1, 0) == DATA_BASE && size >= MINIMUM_DATA && size <= MAXIMUM_DATA &&
           size % 4 == 0 && row_word(header, 1, 2) == DATA_BASE + size;
}

static bool other_rows_are_valid(const uint8_t *header)
{
    for (uint32_t word = 0; word < 4; word++) {
        if (row_word(header, 2, word) != ROW_2[word]) {
            return false;
        }
    }
    for (uint32_t row = 3; row < TABLE_ROWS; row++) {
        for (uint32_t word = 0; word < 4; word++) {
            if (row_word(header, row, word) != 0) {
                return false;
            }
        }
    }
    return true;
}

static bool vectors_are_valid(const UpdateFile *file, uint32_t code_size)
{
    uint32_t code[2];
    uint32_t entry[2];
    if (!file->read(file->context, CODE_OFFSET, code, VECTOR_BYTES) ||
        !file->read(file->context, CODE_OFFSET + code_size, entry, VECTOR_BYTES)) {
        return false;
    }
    uint32_t stack = le32((const uint8_t *)&entry[0]);
    uint32_t reset = le32((const uint8_t *)&entry[1]);
    uint32_t handler = reset & ~1u;
    return stack == DATA_BASE && (reset & 1u) != 0 && handler >= CODE_BASE &&
           handler < CODE_BASE + code_size && code[0] == entry[0] && code[1] == entry[1];
}

static bool layout_is_bootable(const UpdateFile *file, const uint8_t *header)
{
    uint32_t code_size = 0;
    return code_row_is_valid(header, &code_size) && data_row_is_valid(header) &&
           other_rows_are_valid(header) && vectors_are_valid(file, code_size);
}

static bool header_copy_matches(const UpdateFile *file, const uint32_t *first_chunk)
{
    uint32_t copy[HEADER_BYTES / 4];
    return file->read(file->context, FW_SIZE, copy, HEADER_BYTES) &&
           same_bytes(copy, first_chunk, HEADER_BYTES / 4);
}

static bool chunk_matches_slot(const UpdateFile *file, const UpdateStorage *storage, uint32_t slot,
                               uint32_t chunk)
{
    return read_file_chunk(file, chunk, source_chunk) &&
           read_slot_chunk(storage, slot, chunk, target_chunk) &&
           same_bytes(source_chunk, target_chunk, CHUNK_WORDS);
}

static bool matches_installed(const UpdateFile *file, const UpdateStorage *storage,
                              uint32_t fw1_end)
{
    uint32_t used_chunks = (fw1_end + UPDATER_CHUNK_BYTES - 1) / UPDATER_CHUNK_BYTES;
    for (uint32_t chunk = 0; chunk < used_chunks; chunk++) {
        if (!chunk_matches_slot(file, storage, storage->slot_1, chunk)) {
            return false;
        }
    }
    return chunk_matches_slot(file, storage, storage->slot_1, FW_SIZE / UPDATER_CHUNK_BYTES) &&
           chunk_matches_slot(file, storage, storage->slot_2, 0);
}

static bool crc_is_valid(const UpdateFile *file)
{
    uint32_t crc = 0;
    for (uint32_t chunk = 0; chunk < CHUNK_COUNT; chunk++) {
        if (!read_file_chunk(file, chunk, source_chunk)) {
            return false;
        }
        crc = image_crc_update(crc, (const uint8_t *)source_chunk, UPDATER_CHUNK_BYTES);
    }
    uint32_t stored = 0;
    return file->read(file->context, FW_END, &stored, sizeof stored) &&
           crc == le32((const uint8_t *)&stored);
}

UpdateCheck updater_check(const UpdateFile *file, const UpdateStorage *storage)
{
    static uint32_t header[PREFIX_BYTES / 4];
    if (file->size != UPDATER_IMAGE_SIZE || !file->read(file->context, 0, header, PREFIX_BYTES)) {
        return UPDATE_INVALID;
    }
    const uint8_t *bytes = (const uint8_t *)header;
    if (!header_is_valid(bytes, storage) || !header_copy_matches(file, header) ||
        !layout_is_bootable(file, bytes)) {
        return UPDATE_INVALID;
    }
    if (matches_installed(file, storage, le32(bytes + FW1_END_OFFSET))) {
        return UPDATE_ALREADY_INSTALLED;
    }
    return crc_is_valid(file) ? UPDATE_VALID : UPDATE_INVALID;
}

static bool read_source(const SlotCopy *copy, uint32_t chunk)
{
    if (copy->file != NULL) {
        return read_file_chunk(copy->file, chunk, source_chunk);
    }
    return read_slot_chunk(copy->storage, copy->source_slot, chunk, source_chunk);
}

static bool write_verified(const SlotCopy *copy, uint32_t chunk, const uint32_t *words)
{
    uint32_t lba = copy->target_slot + chunk * CHUNK_SECTORS;
    return copy->storage->write(copy->storage->context, lba, CHUNK_SECTORS, words) &&
           read_slot_chunk(copy->storage, copy->target_slot, chunk, target_chunk) &&
           same_bytes(words, target_chunk, CHUNK_WORDS);
}

static bool copy_chunk(const SlotCopy *copy, uint32_t chunk)
{
    if (!read_source(copy, chunk) ||
        !read_slot_chunk(copy->storage, copy->target_slot, chunk, target_chunk)) {
        return false;
    }
    if (same_bytes(source_chunk, target_chunk, CHUNK_WORDS)) {
        return true;
    }
    return write_verified(copy, chunk, source_chunk);
}

static bool invalidate_slot(const SlotCopy *copy)
{
    for (uint32_t index = 0; index < CHUNK_WORDS; index++) {
        source_chunk[index] = 0xFFFFFFFFu;
    }
    return write_verified(copy, 0, source_chunk);
}

static void report(const SlotCopy *copy, uint32_t chunk)
{
    if (copy->progress.report != NULL) {
        copy->progress.report(copy->progress.context, copy->pass * CHUNK_COUNT + chunk,
                              PASSES * CHUNK_COUNT);
    }
}

static bool copy_slot(const SlotCopy *copy)
{
    if (!invalidate_slot(copy)) {
        return false;
    }
    for (uint32_t chunk = 1; chunk < CHUNK_COUNT; chunk++) {
        if (!copy_chunk(copy, chunk)) {
            return false;
        }
        report(copy, chunk);
    }
    if (!copy_chunk(copy, 0)) {
        return false;
    }
    report(copy, CHUNK_COUNT);
    return true;
}

bool updater_install(const UpdateFile *file, const UpdateStorage *storage, UpdateProgress progress)
{
    SlotCopy second = {file, storage, 0, storage->slot_2, progress, 0};
    SlotCopy first = {NULL, storage, storage->slot_2, storage->slot_1, progress, 1};
    return copy_slot(&second) && copy_slot(&first);
}

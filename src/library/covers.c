#include "library/covers.h"

enum {
    MAGIC_SIZE = 4,
    VERSION_FIELD = 4,
    COUNT_FIELD = 6,
    FULL_FIELD = 8,
    HALF_FIELD = 10,
    BYTES_PER_PIXEL = 2,
    FULL_BYTES = COVER_FULL_PIXELS * BYTES_PER_PIXEL,
    HALF_BYTES = COVER_HALF_PIXELS * BYTES_PER_PIXEL,
    RECORD_BYTES = FULL_BYTES + HALF_BYTES,
    BITS_PER_BYTE = 8
};

static const uint8_t MAGIC[MAGIC_SIZE] = {'E', 'M', 'C', 'V'};

static uint16_t u16_at(const uint8_t *bytes, uint32_t offset)
{
    return (uint16_t)(bytes[offset] | bytes[offset + 1] << BITS_PER_BYTE);
}

static uint32_t u32_at(const uint8_t *bytes, uint32_t offset)
{
    return (uint32_t)u16_at(bytes, offset) | (uint32_t)u16_at(bytes, offset + 2) << 16;
}

static bool header_valid(const uint8_t *header)
{
    for (int index = 0; index < MAGIC_SIZE; index++) {
        if (header[index] != MAGIC[index]) {
            return false;
        }
    }
    return u16_at(header, VERSION_FIELD) == COVERS_VERSION &&
           u16_at(header, FULL_FIELD) == COVER_FULL_SIZE &&
           u16_at(header, HALF_FIELD) == COVER_HALF_SIZE;
}

bool covers_open(Covers *covers, ByteSource source, uint32_t size)
{
    uint8_t header[COVERS_HEADER_SIZE];
    *covers = (Covers){.source = source, .size = size, .count = 0};
    if (size < COVERS_HEADER_SIZE || !source.read(source.context, 0, header, sizeof header) ||
        !header_valid(header)) {
        return false;
    }
    uint32_t count = u16_at(header, COUNT_FIELD);
    uint64_t needed = COVERS_HEADER_SIZE + (uint64_t)count * (COVERS_ENTRY_SIZE + RECORD_BYTES);
    if (needed > size) {
        return false;
    }
    covers->count = count;
    return true;
}

static bool read_entry(const Covers *covers, uint32_t position, uint32_t *key, uint32_t *record)
{
    uint8_t entry[COVERS_ENTRY_SIZE];
    uint32_t offset = COVERS_HEADER_SIZE + position * COVERS_ENTRY_SIZE;
    if (!covers->source.read(covers->source.context, offset, entry, sizeof entry)) {
        return false;
    }
    *key = u32_at(entry, 0);
    *record = u32_at(entry, 4);
    return true;
}

bool covers_find(const Covers *covers, uint32_t key, uint32_t *record)
{
    uint32_t low = 0;
    uint32_t high = covers->count;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        uint32_t found = 0;
        if (!read_entry(covers, middle, &found, record)) {
            return false;
        }
        if (found == key) {
            return *record <= covers->size && covers->size - *record >= RECORD_BYTES;
        }
        if (found < key) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return false;
}

static void unpack_pixels(uint16_t *pixels, uint32_t count)
{
    const uint8_t *bytes = (const uint8_t *)pixels;
    for (uint32_t index = 0; index < count; index++) {
        uint16_t value = u16_at(bytes, index * BYTES_PER_PIXEL);
        pixels[index] = value;
    }
}

bool covers_read(const Covers *covers, uint32_t record, CoverSize size, uint16_t *pixels)
{
    bool full = size == COVER_FULL;
    uint32_t offset = full ? record : record + FULL_BYTES;
    uint32_t length = full ? FULL_BYTES : HALF_BYTES;
    if (!covers->source.read(covers->source.context, offset, pixels, length)) {
        return false;
    }
    unpack_pixels(pixels, full ? COVER_FULL_PIXELS : COVER_HALF_PIXELS);
    return true;
}

#include "playback/bookmarks.h"

#include "core/hash.h"

enum {
    VERSION = 1,
    MAGIC_SIZE = 4,
    VERSION_FIELD = 4,
    COUNT_FIELD = 6,
    RECORDS_FIELD = 12,
    KEY_OFFSET = 0,
    TRACK_OFFSET = 4,
    POSITION_OFFSET = 8,
    CHAPTER_OFFSET = 12,
    FLAGS_OFFSET = 14,
    CHECKSUM_SIZE = 4,
    BITS_PER_BYTE = 8
};

static const uint8_t MAGIC[MAGIC_SIZE] = {'E', 'M', 'B', 'K'};

static void put_u16(uint8_t *bytes, uint32_t offset, uint32_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> BITS_PER_BYTE);
}

static void put_u32(uint8_t *bytes, uint32_t offset, uint32_t value)
{
    put_u16(bytes, offset, value & 0xFFFFu);
    put_u16(bytes, offset + 2, value >> 16);
}

static uint16_t get_u16(const uint8_t *bytes, uint32_t offset)
{
    return (uint16_t)(bytes[offset] | bytes[offset + 1] << BITS_PER_BYTE);
}

static uint32_t get_u32(const uint8_t *bytes, uint32_t offset)
{
    return (uint32_t)get_u16(bytes, offset) | (uint32_t)get_u16(bytes, offset + 2) << 16;
}

static int find_index(const Bookmarks *bookmarks, uint32_t key)
{
    for (int index = 0; index < bookmarks->count; index++) {
        if (bookmarks->entries[index].key == key) {
            return index;
        }
    }
    return -1;
}

void bookmarks_clear(Bookmarks *bookmarks)
{
    bookmarks->count = 0;
}

bool bookmarks_find(const Bookmarks *bookmarks, uint32_t key, Bookmark *bookmark)
{
    int index = find_index(bookmarks, key);
    if (index < 0) {
        return false;
    }
    *bookmark = bookmarks->entries[index];
    return true;
}

static void shift_down(Bookmarks *bookmarks, int last)
{
    for (int index = last; index > 0; index--) {
        bookmarks->entries[index] = bookmarks->entries[index - 1];
    }
}

void bookmarks_put(Bookmarks *bookmarks, Bookmark bookmark)
{
    int index = find_index(bookmarks, bookmark.key);
    if (index < 0) {
        if (bookmarks->count < BOOKMARKS_CAPACITY) {
            bookmarks->count++;
        }
        index = bookmarks->count - 1;
    }
    shift_down(bookmarks, index);
    bookmarks->entries[0] = bookmark;
}

bool bookmark_finished(Bookmark bookmark)
{
    return (bookmark.flags & BOOKMARK_FINISHED) != 0;
}

static void encode_record(Bookmark bookmark, uint8_t *record)
{
    put_u32(record, KEY_OFFSET, bookmark.key);
    put_u32(record, TRACK_OFFSET, bookmark.track_key);
    put_u32(record, POSITION_OFFSET, bookmark.position_ms);
    put_u16(record, CHAPTER_OFFSET, bookmark.chapter);
    record[FLAGS_OFFSET] = bookmark.flags;
    record[FLAGS_OFFSET + 1] = 0;
}

uint32_t bookmarks_encode(const Bookmarks *bookmarks, uint8_t *bytes)
{
    for (int index = 0; index < MAGIC_SIZE; index++) {
        bytes[index] = MAGIC[index];
    }
    put_u16(bytes, VERSION_FIELD, VERSION);
    put_u16(bytes, COUNT_FIELD, bookmarks->count);
    put_u32(bytes, COUNT_FIELD + 2, 0);
    for (uint16_t index = 0; index < bookmarks->count; index++) {
        encode_record(bookmarks->entries[index],
                      bytes + RECORDS_FIELD + (uint32_t)index * BOOKMARK_RECORD_SIZE);
    }
    uint32_t checksum_field = RECORDS_FIELD + (uint32_t)bookmarks->count * BOOKMARK_RECORD_SIZE;
    put_u32(bytes, checksum_field, hash_update(HASH_START, bytes, checksum_field));
    return checksum_field + CHECKSUM_SIZE;
}

static Bookmark decode_record(const uint8_t *record)
{
    return (Bookmark){.key = get_u32(record, KEY_OFFSET),
                      .track_key = get_u32(record, TRACK_OFFSET),
                      .position_ms = get_u32(record, POSITION_OFFSET),
                      .chapter = get_u16(record, CHAPTER_OFFSET),
                      .flags = record[FLAGS_OFFSET]};
}

static bool header_valid(const uint8_t *bytes, uint32_t length)
{
    if (length < RECORDS_FIELD + CHECKSUM_SIZE) {
        return false;
    }
    for (int index = 0; index < MAGIC_SIZE; index++) {
        if (bytes[index] != MAGIC[index]) {
            return false;
        }
    }
    uint32_t count = get_u16(bytes, COUNT_FIELD);
    return get_u16(bytes, VERSION_FIELD) == VERSION && count <= BOOKMARKS_CAPACITY &&
           length >= RECORDS_FIELD + count * BOOKMARK_RECORD_SIZE + CHECKSUM_SIZE;
}

bool bookmarks_decode(const uint8_t *bytes, uint32_t length, Bookmarks *bookmarks)
{
    bookmarks->count = 0;
    if (!header_valid(bytes, length)) {
        return false;
    }
    uint16_t count = get_u16(bytes, COUNT_FIELD);
    uint32_t checksum_field = RECORDS_FIELD + (uint32_t)count * BOOKMARK_RECORD_SIZE;
    if (get_u32(bytes, checksum_field) != hash_update(HASH_START, bytes, checksum_field)) {
        return false;
    }
    for (uint16_t index = 0; index < count; index++) {
        bookmarks->entries[index] =
            decode_record(bytes + RECORDS_FIELD + (uint32_t)index * BOOKMARK_RECORD_SIZE);
    }
    bookmarks->count = count;
    return true;
}

#include "library/favorites.h"

#include "core/hash.h"

enum {
    VERSION = 1,
    MAGIC_SIZE = 4,
    VERSION_FIELD = 4,
    COUNT_FIELD = 6,
    KEYS_FIELD = 8,
    KEY_SIZE = 4,
    CHECKSUM_SIZE = 4,
    BITS_PER_BYTE = 8
};

static const uint8_t MAGIC[MAGIC_SIZE] = {'E', 'M', 'F', 'V'};

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

static int find_key(const Favorites *favorites, uint32_t key)
{
    for (int index = 0; index < favorites->count; index++) {
        if (favorites->keys[index] == key) {
            return index;
        }
    }
    return -1;
}

static void remove_at(Favorites *favorites, int position)
{
    for (int index = position; index + 1 < favorites->count; index++) {
        favorites->keys[index] = favorites->keys[index + 1];
    }
    favorites->count--;
}

void favorites_clear(Favorites *favorites)
{
    favorites->count = 0;
}

bool favorites_contains(const Favorites *favorites, uint32_t key)
{
    return find_key(favorites, key) >= 0;
}

bool favorites_toggle(Favorites *favorites, uint32_t key)
{
    int position = find_key(favorites, key);
    if (position >= 0) {
        remove_at(favorites, position);
        return false;
    }
    if (favorites->count == FAVORITES_CAPACITY) {
        return false;
    }
    favorites->keys[favorites->count++] = key;
    return true;
}

uint32_t favorites_encode(const Favorites *favorites, uint8_t *bytes)
{
    for (int index = 0; index < MAGIC_SIZE; index++) {
        bytes[index] = MAGIC[index];
    }
    put_u16(bytes, VERSION_FIELD, VERSION);
    put_u16(bytes, COUNT_FIELD, favorites->count);
    uint32_t offset = KEYS_FIELD;
    for (int index = 0; index < favorites->count; index++, offset += KEY_SIZE) {
        put_u32(bytes, offset, favorites->keys[index]);
    }
    put_u32(bytes, offset, hash_update(HASH_START, bytes, offset));
    return offset + CHECKSUM_SIZE;
}

static bool header_valid(const uint8_t *bytes, uint32_t length)
{
    if (length < KEYS_FIELD + CHECKSUM_SIZE) {
        return false;
    }
    for (int index = 0; index < MAGIC_SIZE; index++) {
        if (bytes[index] != MAGIC[index]) {
            return false;
        }
    }
    uint32_t count = get_u16(bytes, COUNT_FIELD);
    return get_u16(bytes, VERSION_FIELD) == VERSION && count <= FAVORITES_CAPACITY &&
           length >= KEYS_FIELD + count * KEY_SIZE + CHECKSUM_SIZE;
}

bool favorites_decode(const uint8_t *bytes, uint32_t length, Favorites *favorites)
{
    favorites->count = 0;
    if (!header_valid(bytes, length)) {
        return false;
    }
    uint16_t count = get_u16(bytes, COUNT_FIELD);
    uint32_t checksum_field = KEYS_FIELD + (uint32_t)count * KEY_SIZE;
    if (get_u32(bytes, checksum_field) != hash_update(HASH_START, bytes, checksum_field)) {
        return false;
    }
    for (uint16_t index = 0; index < count; index++) {
        favorites->keys[index] = get_u32(bytes, KEYS_FIELD + (uint32_t)index * KEY_SIZE);
    }
    favorites->count = count;
    return true;
}

uint16_t favorites_collect_albums(const Favorites *favorites, const Library *library,
                                  uint16_t *albums, uint16_t capacity)
{
    uint16_t found = 0;
    uint32_t album_count = library_count(library, LIBRARY_ALBUMS);
    for (uint32_t album = 0; album < album_count && found < capacity; album++) {
        if (favorites_contains(favorites, library_album_key(library, album))) {
            albums[found++] = (uint16_t)album;
        }
    }
    return found;
}

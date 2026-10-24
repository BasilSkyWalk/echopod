#include "core/hash.h"

uint32_t hash_update(uint32_t hash, const void *bytes, size_t length)
{
    const uint8_t *data = bytes;
    for (size_t index = 0; index < length; index++) {
        hash ^= data[index];
        hash *= HASH_PRIME;
    }
    return hash;
}

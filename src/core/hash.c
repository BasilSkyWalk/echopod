#include "core/hash.h"

#define FNV_PRIME UINT32_C(16777619)

uint32_t hash_update(uint32_t hash, const void *bytes, size_t length)
{
    const uint8_t *data = bytes;
    for (size_t index = 0; index < length; index++) {
        hash ^= data[index];
        hash *= FNV_PRIME;
    }
    return hash;
}

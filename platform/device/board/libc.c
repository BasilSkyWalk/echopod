#include "libc.h"

#include <stdint.h>

void *memcpy(void *destination, const void *source, size_t count)
{
    uint8_t *to = destination;
    const uint8_t *from = source;
    while (count-- > 0) {
        *to++ = *from++;
    }
    return destination;
}

void *memmove(void *destination, const void *source, size_t count)
{
    uint8_t *to = destination;
    const uint8_t *from = source;
    if (to <= from || to >= from + count) {
        return memcpy(destination, source, count);
    }
    while (count-- > 0) {
        to[count] = from[count];
    }
    return destination;
}

void *memset(void *destination, int value, size_t count)
{
    uint8_t *to = destination;
    while (count-- > 0) {
        *to++ = (uint8_t)value;
    }
    return destination;
}

int memcmp(const void *left, const void *right, size_t count)
{
    const uint8_t *a = left;
    const uint8_t *b = right;
    for (size_t index = 0; index < count; index++) {
        if (a[index] != b[index]) {
            return a[index] < b[index] ? -1 : 1;
        }
    }
    return 0;
}

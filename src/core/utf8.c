#include "core/utf8.h"

#include <stdbool.h>

enum {
    MAX_CODEPOINT = 0x10FFFF,
    SURROGATE_FIRST = 0xD800,
    SURROGATE_LAST = 0xDFFF,
    CONTINUATION_PAYLOAD_BITS = 6
};

static const uint32_t MINIMUM_FOR_LENGTH[] = {0, 0, 0x80, 0x800, 0x10000};

static bool is_continuation(unsigned char byte)
{
    return (byte & 0xC0u) == 0x80u;
}

static int sequence_length(unsigned char lead)
{
    if (lead < 0x80u) {
        return 1;
    }
    if ((lead & 0xE0u) == 0xC0u) {
        return 2;
    }
    if ((lead & 0xF0u) == 0xE0u) {
        return 3;
    }
    if ((lead & 0xF8u) == 0xF0u) {
        return 4;
    }
    return 0;
}

static bool is_valid_scalar(uint32_t codepoint, int length)
{
    bool surrogate = codepoint >= SURROGATE_FIRST && codepoint <= SURROGATE_LAST;
    return codepoint >= MINIMUM_FOR_LENGTH[length] && codepoint <= MAX_CODEPOINT && !surrogate;
}

static bool decode_sequence(const unsigned char *bytes, int length, uint32_t *codepoint)
{
    uint32_t value = bytes[0] & (0x7Fu >> length);
    for (int index = 1; index < length; index++) {
        if (!is_continuation(bytes[index])) {
            return false;
        }
        value = (value << CONTINUATION_PAYLOAD_BITS) | (bytes[index] & 0x3Fu);
    }
    *codepoint = value;
    return is_valid_scalar(value, length);
}

uint32_t utf8_next(const char **cursor)
{
    const unsigned char *bytes = (const unsigned char *)*cursor;
    if (bytes[0] == 0) {
        return 0;
    }
    int length = sequence_length(bytes[0]);
    if (length == 1) {
        *cursor += 1;
        return bytes[0];
    }
    uint32_t codepoint = 0;
    if (length == 0 || !decode_sequence(bytes, length, &codepoint)) {
        *cursor += 1;
        return UTF8_REPLACEMENT_CHARACTER;
    }
    *cursor += length;
    return codepoint;
}

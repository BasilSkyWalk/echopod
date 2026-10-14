#include "fat_names.h"

#include "core/utf8.h"

uint32_t fat_fold(uint32_t code_point)
{
    return code_point >= 'a' && code_point <= 'z' ? code_point - 'a' + 'A' : code_point;
}

static bool next_utf16(const uint16_t *units, size_t limit, size_t *index, uint32_t *code_point)
{
    if (*index >= limit || units[*index] == 0 || units[*index] == 0xFFFF) {
        return false;
    }
    uint32_t first = units[(*index)++];
    if (first >= 0xD800 && first < 0xDC00 && *index < limit) {
        uint32_t second = units[*index];
        if (second >= 0xDC00 && second < 0xE000) {
            (*index)++;
            *code_point = 0x10000 + ((first - 0xD800) << 10) + (second - 0xDC00);
            return true;
        }
    }
    *code_point = first;
    return true;
}

bool fat_name_matches(const uint16_t *units, size_t limit, const char *name, size_t length)
{
    const char *cursor = name;
    size_t unit = 0;
    while (cursor < name + length) {
        uint32_t wanted = utf8_next(&cursor);
        uint32_t found = 0;
        if (!next_utf16(units, limit, &unit, &found) || fat_fold(wanted) != fat_fold(found)) {
            return false;
        }
    }
    uint32_t extra = 0;
    return !next_utf16(units, limit, &unit, &extra);
}

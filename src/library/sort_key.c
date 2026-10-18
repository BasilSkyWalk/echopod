#include "library/sort_key.h"

#include <stdbool.h>
#include <stdint.h>

#include "core/utf8.h"
#include "sort_fold_table.h"

enum {
    HANGUL_FIRST = 0xAC00,
    HANGUL_LAST = 0xD7A3,
    HANGUL_LEAD = 0x1100,
    HANGUL_VOWEL = 0x1161,
    HANGUL_TRAIL = 0x11A7,
    HANGUL_VOWELS_TIMES_TRAILS = 588,
    HANGUL_TRAILS = 28
};

typedef struct KeyBuilder {
    char *key;
    size_t capacity;
    size_t length;
    bool full;
} KeyBuilder;

static bool is_space(uint32_t code_point)
{
    return code_point == ' ' || (code_point >= 0x09 && code_point <= 0x0D) ||
           (code_point >= 0x1C && code_point <= 0x1F) || code_point == 0x85 || code_point == 0xA0 ||
           code_point == 0x1680 || (code_point >= 0x2000 && code_point <= 0x200A) ||
           code_point == 0x2028 || code_point == 0x2029 || code_point == 0x202F ||
           code_point == 0x205F || code_point == 0x3000;
}

static void append_bytes(KeyBuilder *builder, const char *bytes, size_t length)
{
    if (builder->full || builder->length + length >= builder->capacity) {
        builder->full = true;
        return;
    }
    for (size_t index = 0; index < length; index++) {
        builder->key[builder->length++] = bytes[index];
    }
}

static void append_code_point(KeyBuilder *builder, uint32_t code_point)
{
    char bytes[4];
    size_t length = 0;
    if (code_point < 0x80) {
        bytes[length++] = (char)code_point;
    } else if (code_point < 0x800) {
        bytes[length++] = (char)(0xC0 | code_point >> 6);
        bytes[length++] = (char)(0x80 | (code_point & 0x3F));
    } else if (code_point < 0x10000) {
        bytes[length++] = (char)(0xE0 | code_point >> 12);
        bytes[length++] = (char)(0x80 | (code_point >> 6 & 0x3F));
        bytes[length++] = (char)(0x80 | (code_point & 0x3F));
    } else {
        bytes[length++] = (char)(0xF0 | code_point >> 18);
        bytes[length++] = (char)(0x80 | (code_point >> 12 & 0x3F));
        bytes[length++] = (char)(0x80 | (code_point >> 6 & 0x3F));
        bytes[length++] = (char)(0x80 | (code_point & 0x3F));
    }
    append_bytes(builder, bytes, length);
}

static const SortFold *find_fold(const SortFold *folds, size_t count, uint32_t code_point)
{
    size_t low = 0;
    size_t high = count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (folds[middle].code_point < code_point) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low < count && folds[low].code_point == code_point ? &folds[low] : NULL;
}

static const SortFold *fold_for(uint32_t code_point)
{
    return find_fold(SORT_FOLDS, SORT_FOLD_COUNT, code_point);
}

static void append_hangul(KeyBuilder *builder, uint32_t code_point)
{
    uint32_t index = code_point - HANGUL_FIRST;
    append_code_point(builder, HANGUL_LEAD + index / HANGUL_VOWELS_TIMES_TRAILS);
    append_code_point(builder, HANGUL_VOWEL + index % HANGUL_VOWELS_TIMES_TRAILS / HANGUL_TRAILS);
    if (index % HANGUL_TRAILS != 0) {
        append_code_point(builder, HANGUL_TRAIL + index % HANGUL_TRAILS);
    }
}

static void append_folded(KeyBuilder *builder, uint32_t code_point)
{
    if (code_point >= 'A' && code_point <= 'Z') {
        append_code_point(builder, code_point - 'A' + 'a');
        return;
    }
    if (code_point >= HANGUL_FIRST && code_point <= HANGUL_LAST) {
        append_hangul(builder, code_point);
        return;
    }
    const SortFold *fold = code_point >= 0x80 ? fold_for(code_point) : NULL;
    if (fold != NULL) {
        append_bytes(builder, (const char *)SORT_FOLD_TEXT + fold->offset, fold->length);
    } else {
        append_code_point(builder, code_point);
    }
}

static uint32_t code_point_at(const char *key, size_t *index)
{
    const char *cursor = key + *index;
    uint32_t code_point = utf8_next(&cursor);
    *index = (size_t)(cursor - key);
    return code_point;
}

static size_t skip_spaces(const char *key, size_t index, size_t end)
{
    while (index < end) {
        size_t next = index;
        if (!is_space(code_point_at(key, &next))) {
            return index;
        }
        index = next;
    }
    return end;
}

static size_t content_end(const char *key, size_t length)
{
    size_t end = 0;
    for (size_t index = 0; index < length;) {
        uint32_t code_point = code_point_at(key, &index);
        if (!is_space(code_point)) {
            end = index;
        }
    }
    return end;
}

static size_t without_article(const char *key, size_t start, size_t end)
{
    if (end - start < 4 || key[start] != 't' || key[start + 1] != 'h' || key[start + 2] != 'e') {
        return start;
    }
    size_t after = skip_spaces(key, start + 3, end);
    return after > start + 3 && after < end ? after : start;
}

size_t sort_key_make(const char *text, char *key, size_t capacity)
{
    KeyBuilder builder = {.key = key, .capacity = capacity, .length = 0, .full = false};
    const char *cursor = text;
    while (*cursor != '\0' && !builder.full) {
        append_folded(&builder, utf8_next(&cursor));
    }
    size_t end = content_end(key, builder.length);
    size_t start = without_article(key, skip_spaces(key, 0, end), end);
    for (size_t index = start; index < end; index++) {
        key[index - start] = key[index];
    }
    key[end - start] = '\0';
    return end - start;
}

int sort_key_compare(const char *left, const char *right)
{
    char left_key[SORT_KEY_CAPACITY];
    char right_key[SORT_KEY_CAPACITY];
    sort_key_make(left, left_key, sizeof left_key);
    sort_key_make(right, right_key, sizeof right_key);
    const unsigned char *a = (const unsigned char *)left_key;
    const unsigned char *b = (const unsigned char *)right_key;
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (int)*a - (int)*b;
}

static void append_case_folded(KeyBuilder *builder, uint32_t code_point)
{
    if (code_point >= 'A' && code_point <= 'Z') {
        append_code_point(builder, code_point - 'A' + 'a');
        return;
    }
    const SortFold *fold =
        code_point >= 0x80 ? find_fold(CASE_FOLDS, CASE_FOLD_COUNT, code_point) : NULL;
    if (fold != NULL) {
        append_bytes(builder, (const char *)CASE_FOLD_TEXT + fold->offset, fold->length);
    } else {
        append_code_point(builder, code_point);
    }
}

size_t case_fold_make(const char *text, size_t length, char *folded, size_t capacity)
{
    KeyBuilder builder = {.key = folded, .capacity = capacity, .length = 0, .full = false};
    const char *cursor = text;
    while (cursor < text + length && !builder.full) {
        append_case_folded(&builder, utf8_next(&cursor));
    }
    folded[builder.length] = '\0';
    return builder.length;
}

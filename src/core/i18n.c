#include "core/i18n.h"

#include <stddef.h>

enum {
    MAGIC_SIZE = 4,
    VERSION_FIELD = 4,
    COUNT_FIELD = 6,
    SIZE_FIELD = 8,
    OFFSET_BYTES = 2,
    BITS_PER_BYTE = 8
};

static const uint8_t MAGIC[MAGIC_SIZE] = {'E', 'L', 'N', 'G'};

#ifdef ECHO_BUILTIN_ENGLISH
typedef struct EnglishBlob {
#define STRING(id, text) char id[sizeof text];
#include "core/strings_all.def"
#undef STRING
} EnglishBlob;

static const EnglishBlob ENGLISH_BLOB = {
#define STRING(id, text) text,
#include "core/strings_all.def"
#undef STRING
};

static const uint16_t ENGLISH_OFFSETS[STRING_COUNT] = {
#define STRING(id, text) [id] = offsetof(EnglishBlob, id),
#include "core/strings_all.def"
#undef STRING
};
#endif

static const uint8_t *pack_bytes;

static uint32_t read_u16(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << BITS_PER_BYTE;
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return read_u16(bytes) | read_u16(bytes + OFFSET_BYTES) << (2 * BITS_PER_BYTE);
}

static uint32_t text_base(void)
{
    return I18N_HEADER_BYTES + (uint32_t)STRING_COUNT * OFFSET_BYTES;
}

static bool offsets_are_valid(const uint8_t *pack, uint32_t size)
{
    uint32_t base = text_base();
    for (uint32_t id = 0; id < STRING_COUNT; id++) {
        uint32_t offset = read_u16(pack + I18N_HEADER_BYTES + id * OFFSET_BYTES);
        if (base + offset >= size) {
            return false;
        }
        const uint8_t *cursor = pack + base + offset;
        const uint8_t *end = pack + size;
        while (cursor < end && *cursor != '\0') {
            cursor++;
        }
        if (cursor == end) {
            return false;
        }
    }
    return true;
}

bool i18n_attach(const uint8_t *pack, size_t size)
{
    i18n_detach();
    if (pack == NULL || size < I18N_HEADER_BYTES) {
        return false;
    }
    for (int index = 0; index < MAGIC_SIZE; index++) {
        if (pack[index] != MAGIC[index]) {
            return false;
        }
    }
    uint32_t declared = read_u32(pack + SIZE_FIELD);
    if (read_u16(pack + VERSION_FIELD) != I18N_VERSION ||
        read_u16(pack + COUNT_FIELD) != STRING_COUNT || declared > size ||
        text_base() >= declared || !offsets_are_valid(pack, declared)) {
        return false;
    }
    pack_bytes = pack;
    return true;
}

void i18n_detach(void)
{
    pack_bytes = NULL;
}

const char *i18n_english(StringId id)
{
#ifdef ECHO_BUILTIN_ENGLISH
    return (uint32_t)id < STRING_COUNT ? (const char *)&ENGLISH_BLOB + ENGLISH_OFFSETS[id] : "";
#else
    (void)id;
    return "";
#endif
}

const char *i18n_text(StringId id)
{
    if ((uint32_t)id >= STRING_COUNT) {
        return "";
    }
    if (pack_bytes != NULL) {
        uint32_t offset = read_u16(pack_bytes + I18N_HEADER_BYTES + (uint32_t)id * OFFSET_BYTES);
        const char *text = (const char *)pack_bytes + text_base() + offset;
        if (text[0] != '\0') {
            return text;
        }
    }
    return i18n_english(id);
}

void i18n_format(StringBuilder *builder, StringId pattern, const char *const *arguments,
                 uint32_t count)
{
    for (const char *cursor = i18n_text(pattern); *cursor != '\0'; cursor++) {
        bool placeholder =
            cursor[0] == '{' && cursor[1] >= '0' && cursor[1] <= '9' && cursor[2] == '}';
        if (!placeholder) {
            string_builder_append_char(builder, *cursor);
            continue;
        }
        uint32_t index = (uint32_t)(cursor[1] - '0');
        if (index < count) {
            string_builder_append(builder, arguments[index]);
        }
        cursor += 2;
    }
}

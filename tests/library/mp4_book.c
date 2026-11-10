#include "mp4_book.h"

#include <string.h>

enum {
    UNITS_PER_MS = 10000,
    MDAT_BYTES = 16
};

typedef struct Out {
    uint8_t *bytes;
    uint32_t capacity;
    uint32_t size;
} Out;

static void put8(Out *out, uint8_t value)
{
    if (out->size < out->capacity) {
        out->bytes[out->size] = value;
    }
    out->size++;
}

static void put32(Out *out, uint32_t value)
{
    for (int shift = 24; shift >= 0; shift -= 8) {
        put8(out, (uint8_t)(value >> shift));
    }
}

static void put_text(Out *out, const char *text, uint32_t length)
{
    for (uint32_t index = 0; index < length; index++) {
        put8(out, (uint8_t)text[index]);
    }
}

static uint32_t begin(Out *out, const char *type)
{
    uint32_t start = out->size;
    put32(out, 0);
    put_text(out, type, 4);
    return start;
}

static void end(Out *out, uint32_t start)
{
    uint32_t size = out->size - start;
    for (uint32_t index = 0; index < 4 && start + index < out->capacity; index++) {
        out->bytes[start + index] = (uint8_t)(size >> (24 - 8 * index));
    }
}

uint32_t mp4_book_build(uint8_t *bytes, uint32_t capacity, const Mp4BookChapter *chapters,
                        uint32_t count)
{
    Out out = {.bytes = bytes, .capacity = capacity, .size = 0};
    uint32_t ftyp = begin(&out, "ftyp");
    put_text(&out, "M4B ", 4);
    put32(&out, 0);
    end(&out, ftyp);
    uint32_t moov = begin(&out, "moov");
    uint32_t udta = begin(&out, "udta");
    uint32_t chpl = begin(&out, "chpl");
    put32(&out, 0x01000000u);
    put32(&out, 0);
    put8(&out, (uint8_t)count);
    for (uint32_t index = 0; index < count; index++) {
        uint64_t start = (uint64_t)chapters[index].start_ms * UNITS_PER_MS;
        uint32_t length = (uint32_t)strlen(chapters[index].title);
        put32(&out, (uint32_t)(start >> 32));
        put32(&out, (uint32_t)start);
        put8(&out, (uint8_t)length);
        put_text(&out, chapters[index].title, length);
    }
    end(&out, chpl);
    end(&out, udta);
    end(&out, moov);
    uint32_t mdat = begin(&out, "mdat");
    for (uint32_t index = 0; index < MDAT_BYTES; index++) {
        put8(&out, 0);
    }
    end(&out, mdat);
    return out.size;
}

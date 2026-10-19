#include "image/image_reader.h"

#include <stddef.h>

bool image_reader_init(ImageReader *reader, ImageSource source, ImageArena *arena)
{
    *reader = (ImageReader){.source = source, .failed = false};
    reader->buffer = image_arena_take(arena, IMAGE_READER_BUFFER);
    return reader->buffer != NULL;
}

static uint32_t remaining_in_source(const ImageReader *reader)
{
    uint32_t consumed = reader->position + reader->filled;
    return consumed < reader->source.length ? reader->source.length - consumed : 0;
}

static bool refill(ImageReader *reader)
{
    uint32_t kept = reader->filled - reader->cursor;
    for (uint32_t index = 0; index < kept; index++) {
        reader->buffer[index] = reader->buffer[reader->cursor + index];
    }
    reader->position += reader->cursor;
    reader->cursor = 0;
    reader->filled = kept;
    uint32_t room = IMAGE_READER_BUFFER - kept;
    uint32_t left = remaining_in_source(reader);
    uint32_t count = room < left ? room : left;
    if (count == 0) {
        return false;
    }
    uint32_t offset = reader->source.offset + reader->position + kept;
    if (!reader->source.read(reader->source.context, offset, reader->buffer + kept, count)) {
        reader->failed = true;
        return false;
    }
    reader->filled += count;
    return true;
}

bool image_reader_available(ImageReader *reader, uint32_t count)
{
    while (!reader->failed && reader->filled - reader->cursor < count) {
        if (!refill(reader)) {
            return false;
        }
    }
    return !reader->failed;
}

uint8_t image_reader_peek(ImageReader *reader, uint32_t ahead)
{
    if (!image_reader_available(reader, ahead + 1)) {
        return 0;
    }
    return reader->buffer[reader->cursor + ahead];
}

uint8_t image_reader_byte(ImageReader *reader)
{
    if (!image_reader_available(reader, 1)) {
        reader->failed = true;
        return 0;
    }
    return reader->buffer[reader->cursor++];
}

uint32_t image_reader_be16(ImageReader *reader)
{
    uint32_t high = image_reader_byte(reader);
    return high << 8 | image_reader_byte(reader);
}

uint32_t image_reader_be32(ImageReader *reader)
{
    uint32_t high = image_reader_be16(reader);
    return high << 16 | image_reader_be16(reader);
}

bool image_reader_read(ImageReader *reader, uint8_t *out, uint32_t count)
{
    for (uint32_t index = 0; index < count; index++) {
        out[index] = image_reader_byte(reader);
    }
    return !reader->failed;
}

bool image_reader_skip(ImageReader *reader, uint32_t count)
{
    while (count > 0 && !reader->failed) {
        uint32_t buffered = reader->filled - reader->cursor;
        if (buffered == 0) {
            uint32_t left = remaining_in_source(reader);
            if (count > left) {
                reader->failed = true;
                return false;
            }
            reader->position += reader->filled + count;
            reader->filled = 0;
            reader->cursor = 0;
            return true;
        }
        uint32_t step = buffered < count ? buffered : count;
        reader->cursor += step;
        count -= step;
    }
    return !reader->failed;
}

bool image_reader_at_end(ImageReader *reader)
{
    return reader->filled == reader->cursor && remaining_in_source(reader) == 0;
}

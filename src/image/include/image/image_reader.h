#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "image/image_arena.h"

enum {
    IMAGE_READER_BUFFER = 4096
};

typedef struct ImageSource {
    bool (*read)(void *context, uint32_t offset, void *buffer, uint32_t length);
    void *context;
    uint32_t offset;
    uint32_t length;
} ImageSource;

typedef struct ImageReader {
    ImageSource source;
    uint8_t *buffer;
    uint32_t position;
    uint32_t filled;
    uint32_t cursor;
    bool failed;
} ImageReader;

bool image_reader_init(ImageReader *reader, ImageSource source, ImageArena *arena);
bool image_reader_available(ImageReader *reader, uint32_t count);
uint8_t image_reader_peek(ImageReader *reader, uint32_t ahead);
uint8_t image_reader_byte(ImageReader *reader);
uint32_t image_reader_be16(ImageReader *reader);
uint32_t image_reader_be32(ImageReader *reader);
bool image_reader_read(ImageReader *reader, uint8_t *out, uint32_t count);
bool image_reader_skip(ImageReader *reader, uint32_t count);
bool image_reader_at_end(ImageReader *reader);

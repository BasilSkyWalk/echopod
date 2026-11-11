#pragma once

#include <stdint.h>

typedef struct Mp4BookChapter {
    uint32_t start_ms;
    const char *title;
} Mp4BookChapter;

uint32_t mp4_book_build(uint8_t *bytes, uint32_t capacity, const Mp4BookChapter *chapters,
                        uint32_t count);

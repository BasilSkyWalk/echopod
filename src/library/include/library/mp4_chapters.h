#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "library/byte_source.h"

enum {
    MP4_CHAPTERS_MAX = 255,
    MP4_CHAPTERS_KEPT = 160
};

typedef struct Mp4Chapters {
    uint16_t count;
    uint16_t gap_seconds[MP4_CHAPTERS_KEPT];
} Mp4Chapters;

bool mp4_chapters_read(Mp4Chapters *chapters, ByteSource source, uint32_t size);
uint32_t mp4_chapter_start_ms(const Mp4Chapters *chapters, uint16_t index);
uint16_t mp4_chapter_at(const Mp4Chapters *chapters, uint32_t position_ms);

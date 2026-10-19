#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "image/cover_grid.h"
#include "image/image_arena.h"
#include "image/image_reader.h"

typedef enum CoverImageResult {
    COVER_IMAGE_OK,
    COVER_IMAGE_UNSUPPORTED,
    COVER_IMAGE_CORRUPT,
    COVER_IMAGE_TOO_BIG,
    COVER_IMAGE_NO_MEMORY
} CoverImageResult;

typedef struct CoverImage {
    CoverGrid *grid;
    CoverColor color;
} CoverImage;

CoverImageResult png_decode(ImageReader *reader, ImageArena *arena, CoverImage *image);
CoverImageResult jpeg_decode(ImageReader *reader, ImageArena *arena, CoverImage *image);
CoverImageResult cover_image_decode(ImageSource source, void *memory, uint32_t size, uint16_t *full,
                                    uint16_t *half);

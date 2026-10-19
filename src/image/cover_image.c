#include "image/cover_image.h"

#include <stddef.h>

enum {
    JPEG_FIRST = 0xFF,
    JPEG_SECOND = 0xD8,
    PNG_FIRST = 0x89,
    PNG_SECOND = 'P'
};

static CoverImageResult decode_any(ImageReader *reader, ImageArena *arena, CoverImage *image)
{
    uint8_t first = image_reader_peek(reader, 0);
    uint8_t second = image_reader_peek(reader, 1);
    if (first == JPEG_FIRST && second == JPEG_SECOND) {
        return jpeg_decode(reader, arena, image);
    }
    if (first == PNG_FIRST && second == PNG_SECOND) {
        return png_decode(reader, arena, image);
    }
    return reader->failed ? COVER_IMAGE_CORRUPT : COVER_IMAGE_UNSUPPORTED;
}

CoverImageResult cover_image_decode(ImageSource source, void *memory, uint32_t size, uint16_t *full,
                                    uint16_t *half)
{
    ImageArena arena = image_arena_make(memory, size);
    ImageReader reader;
    CoverImage image = {.grid = image_arena_take(&arena, sizeof(CoverGrid)),
                        .color = COVER_COLOR_GRAY};
    if (image.grid == NULL || !image_reader_init(&reader, source, &arena)) {
        return COVER_IMAGE_NO_MEMORY;
    }
    CoverImageResult result = decode_any(&reader, &arena, &image);
    if (result == COVER_IMAGE_OK) {
        cover_grid_render(image.grid, image.color, full, half);
    }
    return result;
}

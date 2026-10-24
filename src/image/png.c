#include <stddef.h>

#include "image/cover_image.h"
#include "image/inflate.h"

#define CHUNK_IHDR 0x49484452u
#define CHUNK_PLTE 0x504C5445u
#define CHUNK_IDAT 0x49444154u
#define CHUNK_IEND 0x49454E44u

enum {
    SIGNATURE_LENGTH = 8,
    CRC_LENGTH = 4,
    PALETTE_ENTRIES = 256,
    PASS_COUNT = 7,
    ROW_LIMIT = 65536,
    MAX_CHUNK = 0x7FFFFFFF,
    COLOR_GRAY = 0,
    COLOR_RGB = 2,
    COLOR_PALETTE = 3,
    COLOR_GRAY_ALPHA = 4,
    COLOR_RGBA = 6,
    FILTER_NONE = 0,
    FILTER_SUB = 1,
    FILTER_UP = 2,
    FILTER_AVERAGE = 3,
    FILTER_PAETH = 4,
    ZLIB_DEFLATE = 8,
    ZLIB_PRESET_DICTIONARY = 0x20
};

static const uint8_t SIGNATURE[SIGNATURE_LENGTH] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
static const uint8_t PASS_X0[PASS_COUNT] = {0, 4, 0, 2, 0, 1, 0};
static const uint8_t PASS_Y0[PASS_COUNT] = {0, 0, 4, 0, 2, 0, 1};
static const uint8_t PASS_DX[PASS_COUNT] = {8, 8, 4, 4, 2, 2, 1};
static const uint8_t PASS_DY[PASS_COUNT] = {8, 8, 8, 4, 4, 2, 2};

typedef struct PngHeader {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t color;
    bool interlaced;
} PngHeader;

typedef struct PngRows {
    PngHeader header;
    CoverGrid *grid;
    uint8_t palette[PALETTE_ENTRIES][3];
    uint32_t palette_size;
    uint32_t channels;
    uint32_t pixel_bytes;
    uint8_t *current;
    uint8_t *previous;
    uint32_t pass;
    uint32_t pass_columns;
    uint32_t pass_rows;
    uint32_t row_bytes;
    uint32_t row;
    uint32_t filled;
    bool done;
    bool corrupt;
} PngRows;

typedef struct PngStream {
    ImageReader *reader;
    uint32_t left;
} PngStream;

typedef struct PngDecoder {
    PngRows rows;
    PngStream stream;
    Inflate inflate;
} PngDecoder;

static uint32_t channels_of(uint32_t color)
{
    switch (color) {
    case COLOR_GRAY:
    case COLOR_PALETTE:
        return 1;
    case COLOR_GRAY_ALPHA:
        return 2;
    case COLOR_RGB:
        return 3;
    default:
        return 4;
    }
}

static bool depth_allowed(uint32_t color, uint32_t depth)
{
    bool low_depth = depth == 1 || depth == 2 || depth == 4;
    if (color == COLOR_GRAY) {
        return low_depth || depth == 8 || depth == 16;
    }
    if (color == COLOR_PALETTE) {
        return low_depth || depth == 8;
    }
    bool known = color == COLOR_RGB || color == COLOR_GRAY_ALPHA || color == COLOR_RGBA;
    return known && (depth == 8 || depth == 16);
}

static CoverImageResult read_header(ImageReader *reader, PngHeader *header)
{
    uint8_t signature[SIGNATURE_LENGTH];
    if (!image_reader_read(reader, signature, sizeof signature)) {
        return COVER_IMAGE_CORRUPT;
    }
    for (uint32_t index = 0; index < SIGNATURE_LENGTH; index++) {
        if (signature[index] != SIGNATURE[index]) {
            return COVER_IMAGE_UNSUPPORTED;
        }
    }
    uint32_t length = image_reader_be32(reader);
    uint32_t type = image_reader_be32(reader);
    header->width = image_reader_be32(reader);
    header->height = image_reader_be32(reader);
    header->depth = image_reader_byte(reader);
    header->color = image_reader_byte(reader);
    uint32_t methods = image_reader_be16(reader);
    header->interlaced = image_reader_byte(reader) == 1;
    image_reader_skip(reader, CRC_LENGTH);
    if (reader->failed || length != 13 || type != CHUNK_IHDR) {
        return COVER_IMAGE_CORRUPT;
    }
    return methods == 0 && depth_allowed(header->color, header->depth) ? COVER_IMAGE_OK
                                                                       : COVER_IMAGE_UNSUPPORTED;
}

static uint32_t row_bytes_for(const PngHeader *header, uint32_t columns)
{
    uint64_t bits = (uint64_t)columns * channels_of(header->color) * header->depth;
    uint64_t bytes = (bits + 7) / 8;
    return bytes > ROW_LIMIT ? ROW_LIMIT + 1 : (uint32_t)bytes;
}

static uint32_t pass_extent(uint32_t size, uint32_t start, uint32_t step)
{
    return size > start ? (size - start + step - 1) / step : 0;
}

static void clear_row(uint8_t *row, uint32_t length)
{
    for (uint32_t index = 0; index < length; index++) {
        row[index] = 0;
    }
}

static void start_pass(PngRows *rows)
{
    const PngHeader *header = &rows->header;
    while (rows->pass < PASS_COUNT) {
        uint32_t pass = header->interlaced ? rows->pass : PASS_COUNT - 1;
        uint32_t x0 = header->interlaced ? PASS_X0[pass] : 0;
        uint32_t y0 = header->interlaced ? PASS_Y0[pass] : 0;
        uint32_t dx = header->interlaced ? PASS_DX[pass] : 1;
        uint32_t dy = header->interlaced ? PASS_DY[pass] : 1;
        rows->pass_columns = pass_extent(header->width, x0, dx);
        rows->pass_rows = pass_extent(header->height, y0, dy);
        if (rows->pass_columns > 0 && rows->pass_rows > 0) {
            rows->row_bytes = row_bytes_for(header, rows->pass_columns);
            rows->row = 0;
            rows->filled = 0;
            clear_row(rows->previous, rows->row_bytes + 1);
            return;
        }
        rows->pass++;
    }
    rows->done = true;
}

static uint8_t paeth(uint8_t left, uint8_t up, uint8_t up_left)
{
    int32_t estimate = (int32_t)left + up - up_left;
    int32_t to_left = estimate > left ? estimate - left : left - estimate;
    int32_t to_up = estimate > up ? estimate - up : up - estimate;
    int32_t to_up_left = estimate > up_left ? estimate - up_left : up_left - estimate;
    if (to_left <= to_up && to_left <= to_up_left) {
        return left;
    }
    return to_up <= to_up_left ? up : up_left;
}

static bool unfilter(PngRows *rows)
{
    uint8_t *row = rows->current + 1;
    const uint8_t *up = rows->previous + 1;
    uint32_t step = rows->pixel_bytes;
    uint32_t filter = rows->current[0];
    for (uint32_t index = 0; index < rows->row_bytes; index++) {
        uint8_t left = index >= step ? row[index - step] : 0;
        uint8_t up_left = index >= step ? up[index - step] : 0;
        if (filter == FILTER_SUB) {
            row[index] = (uint8_t)(row[index] + left);
        } else if (filter == FILTER_UP) {
            row[index] = (uint8_t)(row[index] + up[index]);
        } else if (filter == FILTER_AVERAGE) {
            row[index] = (uint8_t)(row[index] + ((uint32_t)left + up[index]) / 2);
        } else if (filter == FILTER_PAETH) {
            row[index] = (uint8_t)(row[index] + paeth(left, up[index], up_left));
        } else if (filter != FILTER_NONE) {
            return false;
        }
    }
    return true;
}

static uint32_t sample_at(const PngRows *rows, const uint8_t *row, uint32_t index)
{
    uint32_t depth = rows->header.depth;
    if (depth == 16) {
        return row[index * 2];
    }
    if (depth == 8) {
        return row[index];
    }
    uint32_t bit = index * depth;
    uint32_t mask = (1u << depth) - 1u;
    return (uint32_t)(row[bit / 8] >> (8 - depth - bit % 8)) & mask;
}

static uint32_t gray_level(const PngRows *rows, uint32_t sample)
{
    uint32_t depth = rows->header.depth;
    return depth >= 8 ? sample : sample * 255u / ((1u << depth) - 1u);
}

static bool is_gray(uint32_t color)
{
    return color == COLOR_GRAY || color == COLOR_GRAY_ALPHA;
}

static void add_pixel(PngRows *rows, const uint8_t *row, uint32_t column, uint32_t x)
{
    uint32_t first = column * rows->channels;
    uint32_t color = rows->header.color;
    int32_t values[3] = {0, 0, 0};
    if (is_gray(color)) {
        values[0] = (int32_t)gray_level(rows, sample_at(rows, row, first));
        cover_grid_row_pixel(rows->grid, x, values, 1);
        return;
    }
    if (color == COLOR_PALETTE) {
        uint32_t entry = sample_at(rows, row, first);
        for (uint32_t channel = 0; channel < 3 && entry < rows->palette_size; channel++) {
            values[channel] = rows->palette[entry][channel];
        }
    } else {
        for (uint32_t channel = 0; channel < 3; channel++) {
            values[channel] = (int32_t)sample_at(rows, row, first + channel);
        }
    }
    cover_grid_row_pixel(rows->grid, x, values, 3);
}

static void emit_row(PngRows *rows)
{
    const PngHeader *header = &rows->header;
    uint32_t pass = header->interlaced ? rows->pass : PASS_COUNT - 1;
    uint32_t dx = header->interlaced ? PASS_DX[pass] : 1;
    uint32_t x0 = header->interlaced ? PASS_X0[pass] : 0;
    uint32_t y = (header->interlaced ? PASS_Y0[pass] : 0) +
                 rows->row * (header->interlaced ? PASS_DY[pass] : 1);
    CoverGrid *grid = rows->grid;
    if (y < grid->start_y || y >= grid->start_y + grid->span) {
        return;
    }
    cover_grid_row_start(grid);
    for (uint32_t column = 0; column < rows->pass_columns; column++) {
        uint32_t x = x0 + column * dx;
        if (x >= grid->start_x && x < grid->start_x + grid->span) {
            add_pixel(rows, rows->current + 1, column, x);
        }
    }
    cover_grid_row_finish(grid, y, is_gray(header->color) ? 1 : 3);
}

static bool rows_past_crop(const PngRows *rows)
{
    const CoverGrid *grid = rows->grid;
    return !rows->header.interlaced && rows->row >= grid->start_y + grid->span;
}

static bool finish_row(PngRows *rows)
{
    if (!unfilter(rows)) {
        rows->corrupt = true;
        return false;
    }
    emit_row(rows);
    uint8_t *swap = rows->previous;
    rows->previous = rows->current;
    rows->current = swap;
    rows->filled = 0;
    rows->row++;
    if (rows_past_crop(rows)) {
        rows->done = true;
        return false;
    }
    if (rows->row == rows->pass_rows) {
        rows->pass++;
        start_pass(rows);
    }
    return !rows->done;
}

static bool write_rows(void *context, const uint8_t *bytes, uint32_t length)
{
    PngRows *rows = context;
    uint32_t index = 0;
    while (index < length) {
        if (rows->done) {
            return false;
        }
        uint32_t wanted = rows->row_bytes + 1 - rows->filled;
        uint32_t step = length - index < wanted ? length - index : wanted;
        for (uint32_t byte = 0; byte < step; byte++) {
            rows->current[rows->filled + byte] = bytes[index + byte];
        }
        rows->filled += step;
        index += step;
        if (rows->filled == rows->row_bytes + 1 && !finish_row(rows)) {
            return false;
        }
    }
    return true;
}

static bool next_chunk(PngStream *stream, uint32_t *type)
{
    ImageReader *reader = stream->reader;
    stream->left = image_reader_be32(reader);
    *type = image_reader_be32(reader);
    return !reader->failed && stream->left <= MAX_CHUNK;
}

static bool next_data_byte(void *context, uint8_t *byte)
{
    PngStream *stream = context;
    while (stream->left == 0) {
        uint32_t type = 0;
        if (!image_reader_skip(stream->reader, CRC_LENGTH) || !next_chunk(stream, &type) ||
            type != CHUNK_IDAT) {
            stream->left = 0;
            return false;
        }
    }
    stream->left--;
    *byte = image_reader_byte(stream->reader);
    return !stream->reader->failed;
}

static bool read_palette(PngRows *rows, ImageReader *reader, uint32_t length)
{
    if (length % 3 != 0 || length / 3 > PALETTE_ENTRIES) {
        return false;
    }
    rows->palette_size = length / 3;
    for (uint32_t entry = 0; entry < rows->palette_size; entry++) {
        image_reader_read(reader, rows->palette[entry], 3);
    }
    return image_reader_skip(reader, CRC_LENGTH);
}

static CoverImageResult find_data(PngDecoder *decoder)
{
    PngStream *stream = &decoder->stream;
    for (;;) {
        uint32_t type = 0;
        if (!next_chunk(stream, &type) || type == CHUNK_IEND) {
            return COVER_IMAGE_CORRUPT;
        }
        if (type == CHUNK_IDAT) {
            return COVER_IMAGE_OK;
        }
        bool read = type == CHUNK_PLTE
                        ? read_palette(&decoder->rows, stream->reader, stream->left)
                        : image_reader_skip(stream->reader, stream->left + CRC_LENGTH);
        if (!read) {
            return COVER_IMAGE_CORRUPT;
        }
    }
}

static bool zlib_header(PngStream *stream)
{
    uint8_t method = 0;
    uint8_t flags = 0;
    if (!next_data_byte(stream, &method) || !next_data_byte(stream, &flags)) {
        return false;
    }
    return (method & 0x0F) == ZLIB_DEFLATE && (method >> 4) <= 7 &&
           ((uint32_t)method << 8 | flags) % 31 == 0 && (flags & ZLIB_PRESET_DICTIONARY) == 0;
}

static CoverImageResult prepare_rows(PngRows *rows, ImageArena *arena)
{
    const PngHeader *header = &rows->header;
    uint32_t row_bytes = row_bytes_for(header, header->width);
    if (row_bytes > ROW_LIMIT) {
        return COVER_IMAGE_TOO_BIG;
    }
    rows->channels = channels_of(header->color);
    uint32_t bits = rows->channels * header->depth;
    rows->pixel_bytes = bits < 8 ? 1 : bits / 8;
    rows->current = image_arena_take(arena, row_bytes + 1);
    rows->previous = image_arena_take(arena, row_bytes + 1);
    if (rows->current == NULL || rows->previous == NULL) {
        return COVER_IMAGE_NO_MEMORY;
    }
    rows->pass = 0;
    rows->done = false;
    rows->corrupt = false;
    start_pass(rows);
    return COVER_IMAGE_OK;
}

static CoverImageResult decode_data(PngDecoder *decoder, ImageArena *arena)
{
    InflateInput input = {next_data_byte, &decoder->stream};
    InflateOutput output = {write_rows, &decoder->rows};
    if (!zlib_header(&decoder->stream)) {
        return COVER_IMAGE_CORRUPT;
    }
    if (!inflate_init(&decoder->inflate, input, output, arena)) {
        return COVER_IMAGE_NO_MEMORY;
    }
    InflateResult result = inflate_run(&decoder->inflate);
    if (result == INFLATE_ERROR || decoder->rows.corrupt || !decoder->rows.done) {
        return COVER_IMAGE_CORRUPT;
    }
    return COVER_IMAGE_OK;
}

CoverImageResult png_decode(ImageReader *reader, ImageArena *arena, CoverImage *image)
{
    PngDecoder *decoder = image_arena_take(arena, sizeof *decoder);
    if (decoder == NULL) {
        return COVER_IMAGE_NO_MEMORY;
    }
    decoder->rows = (PngRows){.grid = image->grid, .palette_size = 0};
    decoder->stream = (PngStream){.reader = reader, .left = 0};
    CoverImageResult result = read_header(reader, &decoder->rows.header);
    const PngHeader *header = &decoder->rows.header;
    if (result == COVER_IMAGE_OK && !cover_grid_init(image->grid, header->width, header->height)) {
        result = COVER_IMAGE_TOO_BIG;
    }
    if (result == COVER_IMAGE_OK) {
        result = find_data(decoder);
    }
    if (result == COVER_IMAGE_OK) {
        result = prepare_rows(&decoder->rows, arena);
    }
    if (result == COVER_IMAGE_OK) {
        result = decode_data(decoder, arena);
    }
    bool gray = header->color == COLOR_GRAY || header->color == COLOR_GRAY_ALPHA;
    image->color = gray ? COVER_COLOR_GRAY : COVER_COLOR_RGB;
    return result;
}

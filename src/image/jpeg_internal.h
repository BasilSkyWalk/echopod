#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "image/cover_image.h"

enum {
    JPEG_MAX_COMPONENTS = 3,
    JPEG_TABLES = 4,
    JPEG_BLOCK = 64,
    JPEG_FAST_BITS = 9,
    JPEG_MAX_CODE_BITS = 16
};

typedef struct JpegHuffman {
    uint16_t fast[1 << JPEG_FAST_BITS];
    int32_t max_code[JPEG_MAX_CODE_BITS + 2];
    int32_t offset[JPEG_MAX_CODE_BITS + 1];
    uint8_t values[256];
    bool present;
} JpegHuffman;

typedef struct JpegBits {
    ImageReader *reader;
    uint32_t buffer;
    int32_t count;
    uint32_t padding;
    bool marker;
    bool failed;
} JpegBits;

typedef struct JpegComponent {
    uint32_t id;
    uint32_t h;
    uint32_t v;
    uint32_t quant;
    uint32_t blocks_wide;
    uint32_t blocks_high;
    uint32_t dc_table;
    uint32_t ac_table;
    int32_t predictor;
} JpegComponent;

typedef struct JpegScan {
    uint32_t count;
    uint32_t members[JPEG_MAX_COMPONENTS];
    uint32_t start;
    uint32_t end;
    uint32_t high;
    uint32_t low;
} JpegScan;

typedef enum JpegBlockKind {
    JPEG_BLOCK_WHOLE,
    JPEG_BLOCK_DC_FIRST,
    JPEG_BLOCK_DC_DELTA,
    JPEG_BLOCK_AC_DELTA
} JpegBlockKind;

typedef struct JpegDecoder {
    ImageReader *reader;
    CoverGrid *grid;
    JpegBits bits;
    JpegHuffman dc[JPEG_TABLES];
    JpegHuffman ac[JPEG_TABLES];
    int32_t quant[JPEG_TABLES][JPEG_BLOCK];
    bool quant_present[JPEG_TABLES];
    JpegComponent components[JPEG_MAX_COMPONENTS];
    uint32_t component_count;
    uint32_t width;
    uint32_t height;
    uint32_t h_max;
    uint32_t v_max;
    uint32_t mcus_wide;
    uint32_t mcus_high;
    uint32_t restart_interval;
    uint32_t shift;
    uint32_t outputs;
    int32_t kernel[8][8];
    bool progressive;
    bool frame_seen;
    bool adobe_rgb;
    uint32_t scanned;
    int32_t eob_run;
    int32_t coefficients[JPEG_BLOCK];
} JpegDecoder;

extern const uint8_t JPEG_ZIGZAG[JPEG_BLOCK];

bool jpeg_build_huffman(JpegHuffman *table, const uint8_t *counts, const uint8_t *values,
                        uint32_t total);
void jpeg_bits_reset(JpegBits *bits, ImageReader *reader);
int32_t jpeg_decode_symbol(JpegBits *bits, const JpegHuffman *table);
int32_t jpeg_receive_extend(JpegBits *bits, uint32_t size);
uint32_t jpeg_read_bits(JpegBits *bits, uint32_t count);
void jpeg_skip_entropy(ImageReader *reader);
void jpeg_restart(JpegBits *bits);

void jpeg_prepare_kernel(JpegDecoder *decoder);
bool jpeg_coefficient_used(const JpegDecoder *decoder, uint32_t natural);
void jpeg_output_block(JpegDecoder *decoder, uint32_t component, uint32_t block_x, uint32_t block_y,
                       JpegBlockKind kind);
bool jpeg_block_needed(const JpegDecoder *decoder, uint32_t component, uint32_t block_x,
                       uint32_t block_y);
bool jpeg_block_row_past_crop(const JpegDecoder *decoder, uint32_t component, uint32_t block_y);

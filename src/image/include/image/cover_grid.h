#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    COVER_GRID_SIZE = 112,
    COVER_GRID_HALF = 56,
    COVER_GRID_CELLS = COVER_GRID_SIZE * COVER_GRID_SIZE,
    COVER_GRID_HALF_CELLS = COVER_GRID_HALF * COVER_GRID_HALF,
    COVER_GRID_CHANNELS = 3,
    COVER_GRID_LIMIT = 16384,
    COVER_GRID_WEIGHT_ONE = 512
};

typedef enum CoverColor {
    COVER_COLOR_GRAY,
    COVER_COLOR_RGB,
    COVER_COLOR_YCBCR
} CoverColor;

typedef struct CoverRect {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} CoverRect;

typedef struct CoverGrid {
    uint32_t start_x;
    uint32_t start_y;
    uint32_t span;
    uint32_t sample_width[COVER_GRID_CHANNELS];
    uint32_t sample_height[COVER_GRID_CHANNELS];
    uint32_t totals_x[COVER_GRID_CHANNELS][COVER_GRID_SIZE];
    uint32_t totals_y[COVER_GRID_CHANNELS][COVER_GRID_SIZE];
    int32_t row[COVER_GRID_CHANNELS][COVER_GRID_SIZE];
    int32_t sums[COVER_GRID_CHANNELS][COVER_GRID_CELLS];
} CoverGrid;

bool cover_grid_init(CoverGrid *grid, uint32_t width, uint32_t height);
void cover_grid_set_sampling(CoverGrid *grid, uint32_t channel, uint32_t width, uint32_t height);
bool cover_grid_touches(const CoverGrid *grid, CoverRect rect);
void cover_grid_add(CoverGrid *grid, uint32_t channel, CoverRect rect, int32_t value);
void cover_grid_add_color(CoverGrid *grid, CoverRect rect, const int32_t *values,
                          uint32_t channels);
void cover_grid_row_start(CoverGrid *grid);
void cover_grid_row_pixel(CoverGrid *grid, uint32_t x, const int32_t *values, uint32_t channels);
void cover_grid_row_finish(CoverGrid *grid, uint32_t y, uint32_t channels);
void cover_grid_render(CoverGrid *grid, CoverColor color, uint16_t *full, uint16_t *half);

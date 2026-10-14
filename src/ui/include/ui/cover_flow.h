#pragma once

#include <stdint.h>

#include "gfx/image.h"

enum {
    COVER_FLOW_ONE = 256,
    COVER_FLOW_REACH = 4,
    COVER_FLOW_MAX_VISIBLE = 2 * COVER_FLOW_REACH + 1
};

typedef struct CoverFlowLayout {
    int center_x;
    int center_top;
    int center_size;
    int side_offset;
    int side_spacing;
    int side_width;
    int side_near_height;
    int side_far_height;
} CoverFlowLayout;

Quad cover_flow_quad(const CoverFlowLayout *layout, int32_t relative);
int32_t cover_flow_approach(int32_t position, int32_t target, uint32_t elapsed_ms);
int cover_flow_draw_order(int32_t position, int count, int *albums);

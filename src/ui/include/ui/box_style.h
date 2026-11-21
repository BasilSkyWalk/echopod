#pragma once

#include <stdint.h>

enum {
    BOX_EDGE_TOP = 1,
    BOX_EDGE_BOTTOM = 2,
    BOX_EDGE_LEFT = 4,
    BOX_EDGE_RIGHT = 8,
    BOX_EDGE_ALL = BOX_EDGE_TOP | BOX_EDGE_BOTTOM | BOX_EDGE_LEFT | BOX_EDGE_RIGHT
};

typedef struct BoxStyle {
    uint8_t fill_top;
    uint8_t fill_bottom;
    uint8_t border;
    uint8_t shine;
    uint8_t ink;
    uint8_t border_width;
    uint8_t border_edges;
    uint8_t shine_height;
    uint8_t inset_across;
    uint8_t inset_down;
    uint8_t radius;
    uint8_t see_through;
} BoxStyle;

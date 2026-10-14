#pragma once

#include <stdint.h>

typedef struct MarqueeTiming {
    uint32_t pause_ms;
    uint32_t speed_px_per_s;
    int gap_px;
} MarqueeTiming;

int marquee_offset(MarqueeTiming timing, int text_width, int view_width, uint32_t elapsed_ms);

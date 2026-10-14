#include "ui/marquee.h"

enum {
    MILLISECONDS_PER_SECOND = 1000
};

int marquee_offset(MarqueeTiming timing, int text_width, int view_width, uint32_t elapsed_ms)
{
    if (text_width <= view_width || timing.speed_px_per_s == 0) {
        return 0;
    }
    uint32_t distance_px = (uint32_t)(text_width + timing.gap_px);
    uint32_t scroll_ms = distance_px * MILLISECONDS_PER_SECOND / timing.speed_px_per_s;
    uint32_t phase_ms = elapsed_ms % (timing.pause_ms + scroll_ms);
    if (phase_ms < timing.pause_ms) {
        return 0;
    }
    uint64_t travelled = (uint64_t)(phase_ms - timing.pause_ms) * timing.speed_px_per_s;
    return (int)(travelled / MILLISECONDS_PER_SECOND);
}

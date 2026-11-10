#include "ui/marquee.h"

#include <stdbool.h>

enum {
    MILLISECONDS_PER_SECOND = 1000
};

static uint32_t frame_wait_ms;

static bool scrolls(MarqueeTiming timing, int text_width, int view_width)
{
    return text_width > view_width && timing.speed_px_per_s > 0;
}

static uint32_t cycle_ms(MarqueeTiming timing, int text_width)
{
    uint32_t distance_px = (uint32_t)(text_width + timing.gap_px);
    return timing.pause_ms + distance_px * MILLISECONDS_PER_SECOND / timing.speed_px_per_s;
}

static uint32_t pixels_after(MarqueeTiming timing, uint32_t phase_ms)
{
    if (phase_ms < timing.pause_ms) {
        return 0;
    }
    uint64_t travelled = (uint64_t)(phase_ms - timing.pause_ms) * timing.speed_px_per_s;
    return (uint32_t)(travelled / MILLISECONDS_PER_SECOND);
}

uint32_t marquee_wait_ms(MarqueeTiming timing, int text_width, int view_width, uint32_t elapsed_ms)
{
    if (!scrolls(timing, text_width, view_width)) {
        return 0;
    }
    uint32_t cycle = cycle_ms(timing, text_width);
    uint32_t phase_ms = elapsed_ms % cycle;
    uint64_t next_px = (uint64_t)pixels_after(timing, phase_ms) + 1;
    uint32_t speed = timing.speed_px_per_s;
    uint64_t next_ms = timing.pause_ms + (next_px * MILLISECONDS_PER_SECOND + speed - 1) / speed;
    return (uint32_t)(next_ms < cycle ? next_ms : cycle) - phase_ms;
}

void marquee_frame_request(uint32_t wait_ms)
{
    if (frame_wait_ms == 0 || wait_ms < frame_wait_ms) {
        frame_wait_ms = wait_ms;
    }
}

int marquee_offset(MarqueeTiming timing, int text_width, int view_width, uint32_t elapsed_ms)
{
    if (!scrolls(timing, text_width, view_width)) {
        return 0;
    }
    marquee_frame_request(marquee_wait_ms(timing, text_width, view_width, elapsed_ms));
    return (int)pixels_after(timing, elapsed_ms % cycle_ms(timing, text_width));
}

void marquee_frame_start(void)
{
    frame_wait_ms = 0;
}

uint32_t marquee_frame_wait_ms(void)
{
    return frame_wait_ms;
}

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "draw_profile.h"
#include "gfx/canvas.h"
#include "hal/audio.h"
#include "ui/theme.h"

enum {
    FRAME_OVERLAY_TEXT_CAPACITY = 32
};

typedef struct FrameBreakdown {
    uint32_t total_us;
    uint32_t update_us;
    uint32_t draw_us;
    uint32_t send_us;
} FrameBreakdown;

Rect frame_overlay_area(const Theme *theme);
Color frame_overlay_background(void);
void frame_overlay_format(FrameBreakdown breakdown, char *text, size_t capacity);
void frame_overlay_format_parts(const uint32_t *parts_us, char *text, size_t capacity);
void frame_overlay_format_audio(HalAudioStats stats, char *text, size_t capacity);
void frame_overlay_format_block_times(HalAudioStats stats, char *text, size_t capacity);
void frame_overlay_draw(Canvas *canvas, const Theme *theme, FrameBreakdown breakdown,
                        HalAudioStats stats);

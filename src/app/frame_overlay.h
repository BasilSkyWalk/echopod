#pragma once

#include "core/frame_stats.h"
#include "gfx/canvas.h"
#include "ui/theme.h"

Rect frame_overlay_area(void);
Color frame_overlay_background(void);
void frame_overlay_draw(Canvas *canvas, const Theme *theme, const FrameStats *stats);

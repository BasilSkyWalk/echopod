#include "frame_overlay.h"

#include "core/format.h"
#include "hal/display.h"
#include "ui/label.h"

enum {
    OVERLAY_WIDTH = 64,
    OVERLAY_HEIGHT = 18,
    OVERLAY_MARGIN = 4,
    TEXT_CAPACITY = 16
};

static const Color OVERLAY_TEXT = COLOR_RGB(124, 252, 0);

Rect frame_overlay_area(void)
{
    return (Rect){HAL_DISPLAY_WIDTH - OVERLAY_WIDTH - OVERLAY_MARGIN,
                  HAL_DISPLAY_HEIGHT - OVERLAY_HEIGHT - OVERLAY_MARGIN, OVERLAY_WIDTH,
                  OVERLAY_HEIGHT};
}

Color frame_overlay_background(void)
{
    return COLOR_RGB(0, 0, 0);
}

void frame_overlay_draw(Canvas *canvas, const Theme *theme, const FrameStats *stats)
{
    Rect area = frame_overlay_area();
    if (rect_is_empty(rect_intersection(area, canvas->clip))) {
        return;
    }
    char text[TEXT_CAPACITY];
    format_milliseconds(frame_stats_average_us(stats), text, sizeof text);
    TextStyle style = {.font = theme->fonts.small, .color = OVERLAY_TEXT};
    canvas_fill_rect(canvas, area, frame_overlay_background());
    label_draw(canvas, style,
               (Label){.area = label_centered_area(style.font, area, text), .text = text});
}

#include "frame_overlay.h"

#include "core/string_builder.h"
#include "hal/display.h"
#include "ui/label.h"

enum {
    OVERLAY_WIDTH = 120,
    OVERLAY_LINE_HEIGHT = 18,
    OVERLAY_LINES = 2,
    OVERLAY_HEIGHT = OVERLAY_LINE_HEIGHT * OVERLAY_LINES,
    OVERLAY_MARGIN = 4,
    MICROSECONDS_PER_MILLISECOND = 1000
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

static void append_milliseconds(StringBuilder *builder, const char *prefix, uint32_t microseconds)
{
    string_builder_append(builder, prefix);
    string_builder_append_unsigned(builder, (microseconds + MICROSECONDS_PER_MILLISECOND / 2) /
                                                MICROSECONDS_PER_MILLISECOND);
}

void frame_overlay_format(FrameBreakdown breakdown, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    append_milliseconds(&builder, "", breakdown.total_us);
    append_milliseconds(&builder, " u", breakdown.update_us);
    append_milliseconds(&builder, " d", breakdown.draw_us);
    append_milliseconds(&builder, " s", breakdown.send_us);
}

void frame_overlay_format_audio(HalAudioStats stats, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    string_builder_append(&builder, "b");
    string_builder_append_unsigned(&builder, stats.buffered_ms);
    string_builder_append(&builder, " x");
    string_builder_append_unsigned(&builder, stats.underruns);
    append_milliseconds(&builder, " d", stats.decode_max_us);
    append_milliseconds(&builder, " c", stats.read_max_us);
}

static void draw_line(Canvas *canvas, const Theme *theme, Rect area, const char *text)
{
    TextStyle style = {.font = theme->fonts.small, .color = OVERLAY_TEXT};
    label_draw(canvas, style,
               (Label){.area = label_centered_area(style.font, area, text), .text = text});
}

void frame_overlay_draw(Canvas *canvas, const Theme *theme, FrameBreakdown breakdown,
                        HalAudioStats stats)
{
    Rect area = frame_overlay_area();
    if (rect_is_empty(rect_intersection(area, canvas->clip))) {
        return;
    }
    char text[FRAME_OVERLAY_TEXT_CAPACITY];
    canvas_fill_rect(canvas, area, frame_overlay_background());
    Rect line = {area.x, area.y, area.width, OVERLAY_LINE_HEIGHT};
    frame_overlay_format(breakdown, text, sizeof text);
    draw_line(canvas, theme, line, text);
    line.y += OVERLAY_LINE_HEIGHT;
    frame_overlay_format_audio(stats, text, sizeof text);
    draw_line(canvas, theme, line, text);
}

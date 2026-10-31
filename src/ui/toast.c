#include "ui/toast.h"

#include "core/deadline.h"
#include "ui/label.h"

enum {
    TOAST_HEIGHT = 24,
    TOAST_BOTTOM_MARGIN = 10,
    TOAST_SIDE_MARGIN = 4,
    BORDER = 1
};

void toast_show(Toast *toast, const char *text, uint32_t now_ms)
{
    *toast = (Toast){.text = text, .until_ms = now_ms + TOAST_DURATION_MS, .visible = true};
}

void toast_update(Toast *toast, uint32_t now_ms)
{
    if (toast->visible && deadline_reached(now_ms, toast->until_ms)) {
        toast->visible = false;
    }
}

Rect toast_area(const Toast *toast, const Theme *theme)
{
    const ThemeMetrics *metrics = &theme->metrics;
    int width = text_width(theme->fonts.body, toast->text) + 2 * metrics->padding;
    int widest = metrics->screen_width - 2 * TOAST_SIDE_MARGIN;
    width = width < widest ? width : widest;
    int height = TOAST_HEIGHT < metrics->screen_height ? TOAST_HEIGHT : metrics->screen_height;
    int bottom_margin =
        metrics->screen_height > height + TOAST_BOTTOM_MARGIN ? TOAST_BOTTOM_MARGIN : 0;
    return (Rect){(metrics->screen_width - width) / 2,
                  metrics->screen_height - height - bottom_margin, width, height};
}

void toast_draw(const Toast *toast, Canvas *canvas, const Theme *theme)
{
    if (!toast->visible) {
        return;
    }
    Rect area = toast_area(toast, theme);
    Rect inner = {area.x + BORDER, area.y + BORDER, area.width - 2 * BORDER,
                  area.height - 2 * BORDER};
    TextStyle style = {.font = theme->fonts.body, .color = theme->colors.header_text};
    canvas_fill_rect(canvas, area, theme->colors.divider);
    canvas_fill_rect(canvas, inner, theme->colors.header_background);
    Rect text_area = label_centered_area(style.font, inner, toast->text);
    label_draw(canvas, style, (Label){.area = text_area, .text = toast->text});
}

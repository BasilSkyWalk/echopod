#include "ui/status_bar.h"

#include "core/string_builder.h"
#include "gfx/text.h"
#include "ui/icons.h"
#include "ui/label.h"

enum {
    BORDER = 1,
    FILL_INSET = 2,
    NUB_DIVISOR = 5,
    FULL_PERCENT = 100
};

static int icon_top(const Theme *theme)
{
    return (theme->metrics.header_height - theme->metrics.status_icon_size) / 2;
}

static int nub_width(int size)
{
    return size / NUB_DIVISOR > 0 ? size / NUB_DIVISOR : 1;
}

Rect status_bar_playback_area(const Theme *theme)
{
    int size = theme->metrics.status_icon_size;
    return (Rect){theme->metrics.padding, icon_top(theme), size, size};
}

Rect status_bar_battery_area(const Theme *theme)
{
    int size = theme->metrics.status_icon_size;
    int width = 2 * size + nub_width(size);
    return (Rect){theme->metrics.screen_width - theme->metrics.padding - width, icon_top(theme),
                  width, size};
}

int status_bar_battery_fill_width(uint8_t percent, int inner_width)
{
    int clamped = percent < FULL_PERCENT ? percent : FULL_PERCENT;
    int width = clamped * inner_width / FULL_PERCENT;
    return clamped > 0 && width == 0 ? 1 : width;
}

void status_bar_percent_text(uint8_t percent, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    string_builder_append_unsigned(&builder, percent < FULL_PERCENT ? percent : FULL_PERCENT);
    string_builder_append_char(&builder, '%');
}

static int percent_gap(const Theme *theme)
{
    return theme->metrics.padding / 2 > 0 ? theme->metrics.padding / 2 : 1;
}

Rect status_bar_percent_area(const Theme *theme, const char *text)
{
    Rect battery = status_bar_battery_area(theme);
    int width = text_width(theme->fonts.small, text);
    return (Rect){battery.x - percent_gap(theme) - width, battery.y, width, battery.height};
}

static void draw_percent(Canvas *canvas, const Theme *theme, uint8_t percent)
{
    char text[STATUS_BAR_PERCENT_CAPACITY];
    status_bar_percent_text(percent, text, sizeof text);
    Rect area = status_bar_percent_area(theme, text);
    TextStyle style = {.font = theme->fonts.small, .color = theme->colors.header_text};
    label_draw(canvas, style,
               (Label){.area = label_centered_area(style.font, area, text), .text = text});
}

static Color fill_color(const Theme *theme, StatusBarState state)
{
    if (state.charging) {
        return theme->colors.success;
    }
    return state.battery_percent <= STATUS_BAR_LOW_BATTERY_PERCENT ? theme->colors.danger
                                                                   : theme->colors.header_text;
}

static void draw_outline(Canvas *canvas, Rect body, Color color)
{
    canvas_fill_rect(canvas, (Rect){body.x, body.y, body.width, BORDER}, color);
    canvas_fill_rect(canvas, (Rect){body.x, body.y + body.height - BORDER, body.width, BORDER},
                     color);
    canvas_fill_rect(canvas, (Rect){body.x, body.y, BORDER, body.height}, color);
    canvas_fill_rect(canvas, (Rect){body.x + body.width - BORDER, body.y, BORDER, body.height},
                     color);
}

static void draw_battery(Canvas *canvas, const Theme *theme, StatusBarState state)
{
    Rect area = status_bar_battery_area(theme);
    int size = theme->metrics.status_icon_size;
    Rect body = {area.x, area.y, 2 * size, size};
    int nub = nub_width(size);
    Rect tip = {body.x + body.width, area.y + size / 3, nub, size - 2 * (size / 3)};
    Rect inner = {body.x + FILL_INSET, body.y + FILL_INSET, body.width - 2 * FILL_INSET,
                  body.height - 2 * FILL_INSET};
    Color color = theme->colors.header_text;
    draw_outline(canvas, body, color);
    canvas_fill_rect(canvas, tip, color);
    inner.width = status_bar_battery_fill_width(state.battery_percent, inner.width);
    canvas_fill_rect(canvas, inner, fill_color(theme, state));
}

void status_bar_draw(Canvas *canvas, const Theme *theme, StatusBarState state)
{
    Rect icon = status_bar_playback_area(theme);
    Color color = theme->colors.header_text;
    if (state.playback == STATUS_PLAYBACK_PLAYING) {
        icon_draw_play(canvas, icon, color);
    } else if (state.playback == STATUS_PLAYBACK_PAUSED) {
        icon_draw_pause(canvas, icon, color);
    }
    if (state.battery_known) {
        draw_battery(canvas, theme, state);
        draw_percent(canvas, theme, state.battery_percent);
    }
}

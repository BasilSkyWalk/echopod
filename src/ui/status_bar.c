#include "ui/status_bar.h"

#include "core/string_builder.h"
#include "gfx/text.h"
#include "ui/icons.h"
#include "ui/label.h"

enum {
    BORDER = 1,
    FILL_INSET = 2,
    NUB_DIVISOR = 5,
    BOLT_WIDTH_TENTHS = 6,
    BLUETOOTH_WIDTH_TENTHS = 7,
    TENTHS = 10,
    FULL_PERCENT = 100,
    CLOCK_TEXT_CAPACITY = 16
};

static const Font *status_font(const Theme *theme)
{
    return theme->fonts.status != NULL ? theme->fonts.status : theme->fonts.small;
}

static int icon_top(const Theme *theme)
{
    int band = theme->metrics.status_height > 0 ? theme->metrics.status_height
                                                : theme->metrics.header_height;
    return (band - theme->metrics.status_icon_size) / 2;
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

Rect status_bar_bluetooth_area(const Theme *theme)
{
    Rect playback = status_bar_playback_area(theme);
    int width = playback.width * BLUETOOTH_WIDTH_TENTHS / TENTHS;
    return (Rect){playback.x + playback.width + theme->metrics.padding / 2, playback.y,
                  width > 0 ? width : 1, playback.height};
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

Rect status_bar_bolt_area(const Theme *theme)
{
    Rect battery = status_bar_battery_area(theme);
    int width = theme->metrics.status_icon_size * BOLT_WIDTH_TENTHS / TENTHS;
    int bolt_width = width > 0 ? width : 1;
    return (Rect){battery.x - percent_gap(theme) - bolt_width, battery.y, bolt_width,
                  battery.height};
}

Rect status_bar_percent_area(const Theme *theme, const char *text, bool charging)
{
    Rect beside = charging ? status_bar_bolt_area(theme) : status_bar_battery_area(theme);
    int width = text_width(status_font(theme), text);
    return (Rect){beside.x - percent_gap(theme) - width, beside.y, width, beside.height};
}

static int right_items_left(const Theme *theme, StatusBarState state)
{
    if (!state.battery_known) {
        return theme->metrics.screen_width - theme->metrics.padding;
    }
    if (!state.hide_percent) {
        return status_bar_percent_area(theme, "100%", state.charging).x;
    }
    return state.charging ? status_bar_bolt_area(theme).x : status_bar_battery_area(theme).x;
}

static char widest_digit(const Font *font)
{
    char widest = '0';
    int widest_width = 0;
    for (char digit = '0'; digit <= '9'; digit++) {
        char text[2] = {digit, '\0'};
        int width = text_width(font, text);
        if (width > widest_width) {
            widest_width = width;
            widest = digit;
        }
    }
    return widest;
}

static int clock_room(const Font *font, const char *clock)
{
    char widest[CLOCK_TEXT_CAPACITY];
    char digit = widest_digit(font);
    size_t index = 0;
    for (; clock[index] != '\0' && index + 1 < sizeof widest; index++) {
        bool is_digit = clock[index] >= '0' && clock[index] <= '9';
        widest[index] = is_digit ? digit : clock[index];
    }
    widest[index] = '\0';
    return text_width(font, widest);
}

Rect status_bar_clock_area(const Theme *theme, StatusBarState state)
{
    Rect bluetooth = status_bar_bluetooth_area(theme);
    int gap = percent_gap(theme);
    int left = bluetooth.x + bluetooth.width + gap;
    int right = right_items_left(theme, state) - gap;
    int width = state.clock != NULL ? clock_room(status_font(theme), state.clock) : 0;
    bool centered = theme->metrics.status_height > 0;
    int x = centered ? (theme->metrics.screen_width - width) / 2 : left;
    int limit = centered ? right : theme->metrics.header_inset;
    if (centered && x + width > limit) {
        x = left;
    }
    if (width == 0 || x < left || x + width > limit) {
        return (Rect){0, 0, 0, 0};
    }
    return (Rect){x, bluetooth.y, width, bluetooth.height};
}

static void draw_percent(Canvas *canvas, const Theme *theme, uint8_t percent, bool charging)
{
    char text[STATUS_BAR_PERCENT_CAPACITY];
    status_bar_percent_text(percent, text, sizeof text);
    Rect area = status_bar_percent_area(theme, text, charging);
    TextStyle style = {.font = status_font(theme), .color = theme->colors.header_text};
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
    if (state.clock != NULL && theme->metrics.status_height > 0 &&
        rect_is_empty(status_bar_clock_area(theme, state))) {
        state.hide_percent = true;
    }
    Rect icon = status_bar_playback_area(theme);
    Color color = theme->colors.header_text;
    if (state.playback == STATUS_PLAYBACK_PLAYING) {
        icon_draw_play(canvas, icon, color);
    } else if (state.playback == STATUS_PLAYBACK_PAUSED) {
        icon_draw_pause(canvas, icon, color);
    }
    if (state.bluetooth != STATUS_BLUETOOTH_OFF) {
        Color tint =
            state.bluetooth == STATUS_BLUETOOTH_CONNECTED ? color : theme->colors.text_tertiary;
        icon_draw_bluetooth(canvas, status_bar_bluetooth_area(theme), tint);
    }
    Rect clock = status_bar_clock_area(theme, state);
    if (!rect_is_empty(clock)) {
        TextStyle style = {.font = status_font(theme), .color = color};
        label_draw(canvas, style,
                   (Label){.area = label_centered_area(style.font, clock, state.clock),
                           .text = state.clock});
    }
    if (state.battery_known) {
        draw_battery(canvas, theme, state);
        if (state.charging) {
            icon_draw_bolt(canvas, status_bar_bolt_area(theme), theme->colors.success);
        }
        if (!state.hide_percent) {
            draw_percent(canvas, theme, state.battery_percent, state.charging);
        }
    }
}

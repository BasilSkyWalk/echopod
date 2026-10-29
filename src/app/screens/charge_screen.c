#include "charge_screen.h"

#include "core/string_builder.h"
#include "ui/icons.h"
#include "ui/label.h"

enum {
    BODY_WIDTH = 88,
    BODY_HEIGHT = 44,
    BORDER = 3,
    INSET = 3,
    NUB_WIDTH = 6,
    NUB_HEIGHT = 18,
    BOLT_WIDTH = 16,
    BOLT_HEIGHT = 26,
    TEXT_GAP = 14,
    TEXT_HEIGHT = 22,
    HINT_BOTTOM = 10,
    HINT_HEIGHT = 16,
    FULL_PERCENT = 100,
    TEXT_CAPACITY = 24
};

static void draw_outline(Canvas *canvas, Rect body, Color color)
{
    canvas_fill_rect(canvas, (Rect){body.x, body.y, body.width, BORDER}, color);
    canvas_fill_rect(canvas, (Rect){body.x, body.y + body.height - BORDER, body.width, BORDER},
                     color);
    canvas_fill_rect(canvas, (Rect){body.x, body.y, BORDER, body.height}, color);
    canvas_fill_rect(canvas, (Rect){body.x + body.width - BORDER, body.y, BORDER, body.height},
                     color);
}

static void draw_battery(Canvas *canvas, const Theme *theme, Rect body, HalBattery battery)
{
    Color outline = theme->colors.text;
    draw_outline(canvas, body, outline);
    canvas_fill_rect(
        canvas,
        (Rect){body.x + body.width, body.y + (body.height - NUB_HEIGHT) / 2, NUB_WIDTH, NUB_HEIGHT},
        outline);
    int edge = BORDER + INSET;
    Rect inner = {body.x + edge, body.y + edge, body.width - 2 * edge, body.height - 2 * edge};
    int percent = battery.known ? battery.percent : 0;
    percent = percent < FULL_PERCENT ? percent : FULL_PERCENT;
    Color fill = battery.charging ? theme->colors.success : theme->colors.text;
    canvas_fill_rect(
        canvas, (Rect){inner.x, inner.y, inner.width * percent / FULL_PERCENT, inner.height}, fill);
    if (battery.charging) {
        Rect bolt = {body.x + (body.width - BOLT_WIDTH) / 2,
                     body.y + (body.height - BOLT_HEIGHT) / 2, BOLT_WIDTH, BOLT_HEIGHT};
        icon_draw_bolt(canvas, bolt, theme->colors.background);
    }
}

void charge_screen_text(HalBattery battery, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (battery.usb_power && !battery.charging) {
        string_builder_append(&builder, "Charged");
        return;
    }
    string_builder_append(&builder, "Charging");
    if (battery.known) {
        string_builder_append_char(&builder, ' ');
        string_builder_append_unsigned(&builder, battery.percent < FULL_PERCENT ? battery.percent
                                                                                : FULL_PERCENT);
        string_builder_append_char(&builder, '%');
    }
}

void charge_screen_draw(Canvas *canvas, const Theme *theme, HalBattery battery)
{
    int width = theme->metrics.screen_width;
    int height = theme->metrics.screen_height;
    canvas_fill_rect(canvas, (Rect){0, 0, width, height}, theme->colors.background);
    int block = BODY_HEIGHT + TEXT_GAP + TEXT_HEIGHT;
    Rect body = {(width - BODY_WIDTH - NUB_WIDTH) / 2, (height - block) / 2, BODY_WIDTH,
                 BODY_HEIGHT};
    draw_battery(canvas, theme, body, battery);
    char text[TEXT_CAPACITY];
    charge_screen_text(battery, text, sizeof text);
    Rect line = {0, body.y + BODY_HEIGHT + TEXT_GAP, width, TEXT_HEIGHT};
    TextStyle style = {.font = theme->fonts.body, .color = theme->colors.text};
    label_draw(canvas, style,
               (Label){.area = label_centered_area(style.font, line, text), .text = text});
    static const char HINT[] = "Hold Power to turn on";
    Rect hint = {0, height - HINT_BOTTOM - HINT_HEIGHT, width, HINT_HEIGHT};
    TextStyle small = {.font = theme->fonts.small, .color = theme->colors.text_tertiary};
    label_draw(canvas, small,
               (Label){.area = label_centered_area(small.font, hint, HINT), .text = HINT});
}

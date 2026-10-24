#include "stopwatch_screen.h"

#include "core/format.h"
#include "core/string_builder.h"
#include "hal/system.h"
#include "ui/header.h"
#include "ui/label.h"
#include "ui/navigation.h"

enum {
    TIME_TOP = 50,
    CLOCK_TOP = 100,
    LINE_HEIGHT = 24
};

static void refresh(StopwatchScreen *stopwatch, uint32_t now_ms)
{
    format_duration(now_ms - stopwatch->started_ms, stopwatch->text, sizeof stopwatch->text);
    StringBuilder builder = string_builder_make(stopwatch->clock, sizeof stopwatch->clock);
    string_builder_append(&builder, "CPU ");
    string_builder_append_unsigned(&builder, hal_system_clock_mhz());
    string_builder_append(&builder, " MHz");
}

static void enter(void *context, uint32_t now_ms)
{
    StopwatchScreen *stopwatch = context;
    stopwatch->started_ms = now_ms;
    refresh(stopwatch, now_ms);
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    (void)context;
    (void)now_ms;
    return navigation_exit_action(event);
}

static void update(void *context, uint32_t now_ms)
{
    refresh(context, now_ms);
}

static void draw_centered(Canvas *canvas, TextStyle style, Rect area, const char *text)
{
    label_draw(canvas, style,
               (Label){.area = label_centered_area(style.font, area, text), .text = text});
}

static void draw(void *context, Canvas *canvas)
{
    const StopwatchScreen *stopwatch = context;
    const Theme *theme = stopwatch->theme;
    Rect content = theme_content_area(theme);
    header_draw(canvas, theme, stopwatch->screen.title);
    TextStyle time = {.font = theme->fonts.title, .color = theme->colors.text};
    TextStyle clock = {.font = theme->fonts.body, .color = theme->colors.text_secondary};
    draw_centered(canvas, time, (Rect){content.x, TIME_TOP, content.width, LINE_HEIGHT},
                  stopwatch->text);
    draw_centered(canvas, clock, (Rect){content.x, CLOCK_TOP, content.width, LINE_HEIGHT},
                  stopwatch->clock);
}

Screen *stopwatch_screen_init(StopwatchScreen *stopwatch, const Theme *theme)
{
    *stopwatch = (StopwatchScreen){.theme = theme};
    stopwatch->screen = (Screen){
        .title = "Stopwatch",
        .stays_on_wake = true,
        .context = stopwatch,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &stopwatch->screen;
}

const char *stopwatch_screen_text(const StopwatchScreen *stopwatch)
{
    return stopwatch->text;
}

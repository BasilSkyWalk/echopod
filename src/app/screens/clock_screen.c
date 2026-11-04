#include "clock_screen.h"

#include "core/i18n.h"
#include "core/string_builder.h"
#include "hal/clock.h"
#include "ui/header.h"
#include "ui/label.h"

enum {
    MINUTES_PER_DAY = 24 * 60,
    MINUTES_PER_HOUR = 60,
    MILLISECONDS_PER_MINUTE = 60000
};

static uint32_t minute_of_day(const ClockScreen *clock, uint32_t now_ms)
{
    uint32_t base = (uint32_t)clock->store->settings.clock_hour * MINUTES_PER_HOUR +
                    clock->store->settings.clock_minute;
    return (base + (now_ms - clock->started_ms) / MILLISECONDS_PER_MINUTE) % MINUTES_PER_DAY;
}

static void append_two_digits(StringBuilder *builder, uint32_t value)
{
    if (value < 10) {
        string_builder_append_char(builder, '0');
    }
    string_builder_append_unsigned(builder, value);
}

void clock_screen_format(const ClockScreen *clock, uint32_t now_ms, char *text, size_t capacity)
{
    uint32_t current = minute_of_day(clock, now_ms);
    StringBuilder builder = string_builder_make(text, capacity);
    append_two_digits(&builder, current / MINUTES_PER_HOUR);
    string_builder_append_char(&builder, ':');
    append_two_digits(&builder, current % MINUTES_PER_HOUR);
}

void clock_screen_commit(ClockScreen *clock, uint32_t now_ms)
{
    uint32_t current = minute_of_day(clock, now_ms);
    clock->store->settings.clock_hour = (uint8_t)(current / MINUTES_PER_HOUR);
    clock->store->settings.clock_minute = (uint8_t)(current % MINUTES_PER_HOUR);
    clock->started_ms = now_ms;
}

static void adjust(ClockScreen *clock, int direction, uint32_t now_ms)
{
    uint32_t current = minute_of_day(clock, now_ms);
    int32_t delta = clock->minute_field ? direction : direction * MINUTES_PER_HOUR;
    int32_t changed = (int32_t)current + delta;
    while (changed < 0) {
        changed += MINUTES_PER_DAY;
    }
    changed %= MINUTES_PER_DAY;
    clock->store->settings.clock_hour = (uint8_t)(changed / MINUTES_PER_HOUR);
    clock->store->settings.clock_minute = (uint8_t)(changed % MINUTES_PER_HOUR);
    clock->started_ms = now_ms;
    if (!settings_store_save(clock->store)) {
        toast_show(clock->toast, i18n_text(STR_SETTINGS_SAVE_FAILED), now_ms);
    }
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    ClockScreen *clock = context;
    clock->now_ms = now_ms;
    if (event.key == KEY_BACK && event.gesture == KEY_GESTURE_PRESS) {
        return screen_action_pop();
    }
    if (event.key == KEY_PLAY && event.gesture == KEY_GESTURE_PRESS) {
        clock->minute_field = !clock->minute_field;
    } else if (event.key == KEY_VOLUME_UP) {
        adjust(clock, 1, now_ms);
    } else if (event.key == KEY_VOLUME_DOWN) {
        adjust(clock, -1, now_ms);
    } else if (event.key == KEY_MENU && event.gesture == KEY_GESTURE_PRESS) {
        return screen_action_pop();
    }
    return screen_action_none();
}

static void update(void *context, uint32_t now_ms)
{
    ClockScreen *clock = context;
    clock->now_ms = now_ms;
    clock_screen_format(clock, now_ms, clock->text, sizeof clock->text);
}

static void draw_centered(Canvas *canvas, TextStyle style, Rect area, const char *text)
{
    label_draw(canvas, style,
               (Label){.area = label_centered_area(style.font, area, text), .text = text});
}

static void draw(void *context, Canvas *canvas)
{
    const ClockScreen *clock = context;
    const Theme *theme = clock->theme;
    Rect content = theme_content_area(theme);
    canvas_fill_rect(canvas, content, theme->colors.background);
    header_draw(canvas, theme, clock->screen.title);
    TextStyle time = {.font = theme->fonts.title, .color = theme->colors.text};
    TextStyle hint = {.font = theme->fonts.small, .color = theme->colors.text_secondary};
    draw_centered(canvas, time, (Rect){content.x, content.y + 22, content.width, 50}, clock->text);
    draw_centered(canvas, hint, (Rect){content.x, content.y + 88, content.width, 20},
                  i18n_text(STR_SETTINGS_CLOCK_HINT));
}

Screen *clock_screen_init(ClockScreen *clock, SettingsStore *store, Toast *toast,
                          const Theme *theme, uint32_t now_ms)
{
    *clock = (ClockScreen){.theme = theme, .store = store, .toast = toast,
                           .started_ms = now_ms, .now_ms = now_ms};
    clock_screen_format(clock, now_ms, clock->text, sizeof clock->text);
    clock->screen = (Screen){.title = i18n_text(STR_MENU_CLOCK),
                             .stays_on_wake = true,
                             .context = clock,
                             .handle_key = handle_key,
                             .update = update,
                             .draw = draw};
    return &clock->screen;
}

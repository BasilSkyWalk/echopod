#include "date_time_screen.h"

#include "core/i18n.h"
#include "core/string_builder.h"
#include "hal/clock.h"
#include "hal/system.h"
#include "ui/header.h"
#include "ui/label.h"
#include "ui/navigation.h"

enum {
    FIELD_YEAR,
    FIELD_MONTH,
    FIELD_DAY,
    FIELD_HOUR,
    FIELD_MINUTE,
    FIELD_ZONE,
    HINT_HEIGHT = 16,
    VALUE_CAPACITY = 12,
    DEFAULT_YEAR = 2026,
    MONTHS_PER_YEAR = 12,
    LAST_HOUR = 23,
    LAST_MINUTE = 59,
    LONGEST_MONTH = 31,
    SECONDS_PER_DAY = 86400
};

static const StringId FIELD_LABELS[DATE_TIME_FIELD_COUNT] = {
    STR_SETTINGS_YEAR, STR_SETTINGS_MONTH,  STR_SETTINGS_DAY,
    STR_SETTINGS_HOUR, STR_SETTINGS_MINUTE, STR_SETTINGS_TIME_ZONE};

typedef struct FieldRange {
    int low;
    int high;
} FieldRange;

static const FieldRange RANGES[DATE_TIME_FIELD_COUNT] = {
    {WALL_CLOCK_FIRST_YEAR, WALL_CLOCK_LAST_YEAR},
    {1, MONTHS_PER_YEAR},
    {1, LONGEST_MONTH},
    {0, LAST_HOUR},
    {0, LAST_MINUTE},
    {1, WALL_CLOCK_ZONE_COUNT - 1}};

static int field_value(const DateTimeScreen *screen, int field)
{
    switch (field) {
    case FIELD_YEAR:
        return screen->civil.year;
    case FIELD_MONTH:
        return screen->civil.month;
    case FIELD_DAY:
        return screen->civil.day;
    case FIELD_HOUR:
        return screen->civil.hour;
    case FIELD_MINUTE:
        return screen->civil.minute;
    default:
        return screen->zone;
    }
}

static void store_value(DateTimeScreen *screen, int field, int value)
{
    switch (field) {
    case FIELD_YEAR:
        screen->civil.year = (uint16_t)value;
        break;
    case FIELD_MONTH:
        screen->civil.month = (uint8_t)value;
        break;
    case FIELD_DAY:
        screen->civil.day = (uint8_t)value;
        break;
    case FIELD_HOUR:
        screen->civil.hour = (uint8_t)value;
        break;
    case FIELD_MINUTE:
        screen->civil.minute = (uint8_t)value;
        break;
    default:
        screen->zone = (uint8_t)value;
        break;
    }
}

static void change(DateTimeScreen *screen, int delta)
{
    FieldRange range = RANGES[screen->field];
    int span = range.high - range.low + 1;
    int value = field_value(screen, screen->field);
    value = range.low + (value - range.low + delta + span) % span;
    store_value(screen, screen->field, value);
    screen->changed = true;
}

static void clamp_day(DateTimeScreen *screen)
{
    while (!wall_clock_civil_valid(&screen->civil) && screen->civil.day > 1) {
        screen->civil.day--;
    }
}

static void save(DateTimeScreen *screen)
{
    const DateTimeConfig *config = &screen->config;
    uint32_t now_ms = hal_clock_milliseconds();
    clamp_day(screen);
    bool kept = clock_store_set(config->clock, &config->store->settings, &screen->civil,
                                screen->zone, now_ms);
    settings_store_save(config->store);
    toast_show(config->toast,
               i18n_text(kept ? STR_SETTINGS_CLOCK_SET : STR_SETTINGS_CLOCK_SET_UNTIL_OFF), now_ms);
}

static ScreenAction leave(DateTimeScreen *screen, ScreenAction action)
{
    if (screen->changed) {
        save(screen);
    }
    return action;
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    (void)now_ms;
    DateTimeScreen *screen = context;
    NavigationIntent intent = navigation_from_key(event).intent;
    switch (intent) {
    case NAVIGATION_NEXT:
    case NAVIGATION_PREVIOUS:
        if (screen->editing) {
            change(screen, intent == NAVIGATION_NEXT ? 1 : -1);
        } else {
            screen->field =
                (screen->field + (intent == NAVIGATION_NEXT ? 1 : -1) + DATE_TIME_FIELD_COUNT) %
                DATE_TIME_FIELD_COUNT;
        }
        return screen_action_none();
    case NAVIGATION_SELECT:
        screen->editing = !screen->editing;
        return screen_action_none();
    case NAVIGATION_BACK:
        if (screen->editing) {
            screen->editing = false;
            return screen_action_none();
        }
        return leave(screen, screen_action_pop());
    case NAVIGATION_HOME:
        return leave(screen, navigation_exit_action(event));
    case NAVIGATION_OPTIONS:
    case NAVIGATION_NONE:
        break;
    }
    return screen_action_none();
}

static void enter(void *context, uint32_t now_ms)
{
    DateTimeScreen *screen = context;
    const DateTimeConfig *config = &screen->config;
    if (!clock_store_now(config->clock, now_ms, &screen->civil)) {
        uint32_t build_day = hal_system_build_day();
        screen->civil = (WallCivil){.year = DEFAULT_YEAR, .month = 1, .day = 1};
        if (build_day > 0) {
            wall_clock_civil_of(build_day * SECONDS_PER_DAY, &screen->civil);
        }
    }
    screen->civil.second = 0;
    screen->zone = config->store->settings.options[OPTION_TIME_ZONE];
    if (screen->zone == WALL_CLOCK_ZONE_UNSET) {
        screen->zone = WALL_CLOCK_ZONE_UTC;
    }
    screen->field = 0;
    screen->editing = false;
    screen->changed = false;
}

static void value_text(const DateTimeScreen *screen, int field, char *text, size_t capacity)
{
    if (field == FIELD_ZONE) {
        wall_clock_zone_text(screen->zone, text, capacity);
        return;
    }
    int value = field_value(screen, field);
    StringBuilder builder = string_builder_make(text, capacity);
    if (value < 10 && field != FIELD_YEAR) {
        string_builder_append_char(&builder, '0');
    }
    string_builder_append_unsigned(&builder, (uint32_t)value);
}

static void draw(void *context, Canvas *canvas)
{
    const DateTimeScreen *screen = context;
    const Theme *theme = screen->config.theme;
    Rect content = theme_content_area(theme);
    int padding = theme->metrics.padding;
    int row_height = (content.height - HINT_HEIGHT) / DATE_TIME_FIELD_COUNT;
    row_height = row_height < theme->metrics.row_height ? row_height : theme->metrics.row_height;
    header_draw(canvas, theme, screen->screen.title);
    for (int field = 0; field < DATE_TIME_FIELD_COUNT; field++) {
        char value[VALUE_CAPACITY];
        Rect row = {content.x, content.y + field * row_height, content.width, row_height};
        bool selected = field == screen->field;
        Color text_color =
            selected && screen->editing ? theme->colors.selection_text : theme->colors.text;
        if (selected) {
            canvas_fill_rect(canvas, row,
                             screen->editing ? theme->colors.selection_background
                                             : theme->colors.header_top);
        }
        value_text(screen, field, value, sizeof value);
        TextStyle label = {.font = theme->fonts.body, .color = text_color};
        Rect label_area = {row.x + padding, row.y, row.width - 2 * padding, row.height};
        label_draw(canvas, label,
                   (Label){.area = label_area, .text = i18n_text(FIELD_LABELS[field])});
        int width = text_width(label.font, value);
        Rect value_area = {row.x + row.width - padding - width, row.y, width, row.height};
        label_draw(canvas, label, (Label){.area = value_area, .text = value});
    }
    TextStyle hint = {.font = theme->fonts.small, .color = theme->colors.text_tertiary};
    const char *text =
        i18n_text(screen->editing ? STR_SETTINGS_TIME_EDIT_HINT : STR_SETTINGS_TIME_PICK_HINT);
    Rect hint_area = {content.x, content.y + content.height - HINT_HEIGHT, content.width,
                      HINT_HEIGHT};
    label_draw(canvas, hint,
               (Label){.area = label_centered_area(hint.font, hint_area, text), .text = text});
}

Screen *date_time_screen_init(DateTimeScreen *screen, DateTimeConfig config)
{
    *screen = (DateTimeScreen){.config = config};
    screen->screen = (Screen){
        .title = i18n_text(STR_SETTINGS_DATE_TIME),
        .context = screen,
        .enter = enter,
        .handle_key = handle_key,
        .draw = draw,
    };
    return &screen->screen;
}

ScreenAction date_time_screen_open(void *context)
{
    DateTimeScreen *screen = context;
    return screen_action_push(&screen->screen);
}

const char *date_time_screen_summary(const void *context)
{
    static char summary[WALL_CLOCK_DATE_TIME_CAPACITY];
    const DateTimeScreen *screen = context;
    WallCivil civil;
    if (!clock_store_now(screen->config.clock, hal_clock_milliseconds(), &civil)) {
        return i18n_text(STR_SETTINGS_NOT_SET);
    }
    wall_clock_date_time_text(&civil, summary, sizeof summary);
    return summary;
}

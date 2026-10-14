#include "sim_options.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct ArgumentCursor {
    int count;
    char **arguments;
    int index;
} ArgumentCursor;

typedef bool (*OptionReader)(ArgumentCursor *cursor, SimOptions *options);

typedef struct OptionSpec {
    const char *name;
    OptionReader read;
} OptionSpec;

static const char *next_value(ArgumentCursor *cursor)
{
    if (cursor->index + 1 >= cursor->count) {
        return NULL;
    }
    cursor->index++;
    return cursor->arguments[cursor->index];
}

static bool parse_number(const char *text, unsigned long minimum, unsigned long maximum,
                         unsigned long *value)
{
    if (text == NULL || text[0] < '0' || text[0] > '9') {
        return false;
    }
    char *end = NULL;
    errno = 0;
    *value = strtoul(text, &end, 10);
    return errno == 0 && *end == '\0' && *value >= minimum && *value <= maximum;
}

static bool read_scale(ArgumentCursor *cursor, SimOptions *options)
{
    unsigned long scale = 0;
    if (!parse_number(next_value(cursor), 1, SIM_MAX_SCALE, &scale)) {
        return false;
    }
    options->scale = (int)scale;
    return true;
}

static bool read_frames(ArgumentCursor *cursor, SimOptions *options)
{
    unsigned long frames = 0;
    if (!parse_number(next_value(cursor), 1, UINT32_MAX, &frames)) {
        return false;
    }
    options->frame_limit = (uint32_t)frames;
    return true;
}

static bool read_headless(ArgumentCursor *cursor, SimOptions *options)
{
    (void)cursor;
    options->headless = true;
    return true;
}

static bool read_screenshot(ArgumentCursor *cursor, SimOptions *options)
{
    options->screenshot_path = next_value(cursor);
    return options->screenshot_path != NULL;
}

static bool read_keys(ArgumentCursor *cursor, SimOptions *options)
{
    options->key_script = next_value(cursor);
    return options->key_script != NULL;
}

static bool read_card(ArgumentCursor *cursor, SimOptions *options)
{
    options->card_directory = next_value(cursor);
    return options->card_directory != NULL;
}

static bool read_system_directory(ArgumentCursor *cursor, SimOptions *options)
{
    options->system_directory = next_value(cursor);
    return options->system_directory != NULL;
}

static bool read_battery(ArgumentCursor *cursor, SimOptions *options)
{
    unsigned long percent = 0;
    if (!parse_number(next_value(cursor), 0, SIM_FULL_BATTERY_PERCENT, &percent)) {
        return false;
    }
    options->battery_percent = (uint8_t)percent;
    return true;
}

static bool read_charging(ArgumentCursor *cursor, SimOptions *options)
{
    (void)cursor;
    options->battery_charging = true;
    return true;
}

static const OptionSpec OPTION_SPECS[] = {
    {"--scale", read_scale},
    {"--frames", read_frames},
    {"--headless", read_headless},
    {"--screenshot", read_screenshot},
    {"--keys", read_keys},
    {"--card", read_card},
    {"--system-dir", read_system_directory},
    {"--battery", read_battery},
    {"--charging", read_charging},
};

static bool read_option(ArgumentCursor *cursor, SimOptions *options)
{
    const char *name = cursor->arguments[cursor->index];
    for (size_t spec = 0; spec < sizeof OPTION_SPECS / sizeof OPTION_SPECS[0]; spec++) {
        if (strcmp(name, OPTION_SPECS[spec].name) == 0) {
            return OPTION_SPECS[spec].read(cursor, options);
        }
    }
    return false;
}

bool sim_options_parse(int argument_count, char **arguments, SimOptions *options)
{
    *options =
        (SimOptions){.scale = SIM_DEFAULT_SCALE, .battery_percent = SIM_DEFAULT_BATTERY_PERCENT};
    ArgumentCursor cursor = {.count = argument_count, .arguments = arguments, .index = 1};
    for (; cursor.index < argument_count; cursor.index++) {
        if (!read_option(&cursor, options)) {
            return false;
        }
    }
    return !options->headless || options->frame_limit > 0;
}

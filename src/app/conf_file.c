#include "app/conf_file.h"

#include <stdbool.h>

#include "hal/storage.h"

enum {
    CONF_FILE_MAX_ROWS = 20,
    CONF_FILE_MAX_BYTES = 640,
    CONF_LINE_MAX = 48,
};

static const char CONF_FILE_PATH[] = "/echo.conf";
static const char CONF_ERROR_TEXT[] = "Error parsing config file";

static const uint8_t CONF_DEFAULTS[CONF_COUNT] = {
    [CONF_DEFAULT_COVER] = 0,
};

static const char *const CONF_KEY_NAMES[CONF_COUNT] = {
    [CONF_DEFAULT_COVER] = "CONF_DEFAULT_COVER",
};

static void reset_to_defaults(EchoConf *conf)
{
    for (int index = 0; index < CONF_COUNT; index++) {
        conf->values[index] = CONF_DEFAULTS[index];
    }
}

static bool key_matches(const char *name, const char *key, uint32_t length)
{
    uint32_t index = 0;
    while (index < length && name[index] == key[index]) {
        index++;
    }
    return index == length && name[index] == '\0';
}

static bool parse_line(const char *line, uint32_t length, EchoConf *conf)
{
    uint32_t equals = 0;
    while (equals < length && line[equals] != '=') {
        char character = line[equals];
        bool letter = (character >= 'A' && character <= 'Z') || character == '_';
        bool digit = equals > 0 && character >= '0' && character <= '9';
        if (!letter && !digit) {
            return false;
        }
        equals++;
    }
    if (equals == 0 || equals == length) {
        return false;
    }

    int matched = -1;
    for (int index = 0; index < CONF_COUNT; index++) {
        if (key_matches(CONF_KEY_NAMES[index], line, equals)) {
            matched = index;
            break;
        }
    }
    if (matched < 0) {
        return true;
    }

    if (equals + 1 == length) {
        return false;
    }

    uint32_t value = 0;
    for (uint32_t index = equals + 1; index < length; index++) {
        char character = line[index];
        if (character < '0' || character > '9') {
            return false;
        }
        value = value * 10 + (uint32_t)(character - '0');
        if (value > 255) {
            return false;
        }
    }
    conf->values[matched] = (uint8_t)value;
    return true;
}

void conf_file_load(EchoConf *conf, Toast *toast, uint32_t now_ms)
{
    reset_to_defaults(conf);

    HalFile file;
    if (!hal_file_open(&file, CONF_FILE_PATH)) {
        return;
    }

    bool ok = file.size > 0 && file.size <= CONF_FILE_MAX_BYTES;
    bool read_failed = false;
    bool parse_failed = false;
    int rows = 0;

    if (ok) {
        char line[CONF_LINE_MAX + 2];
        uint32_t position = 0;

        while (position < file.size && rows < CONF_FILE_MAX_ROWS) {
            uint32_t chunk = file.size - position;
            if (chunk > (uint32_t)(CONF_LINE_MAX + 2)) {
                chunk = (uint32_t)(CONF_LINE_MAX + 2);
            }
            if (!hal_file_read(file, position, line, chunk)) {
                read_failed = true;
                break;
            }

            uint32_t newline = 0;
            while (newline < chunk && line[newline] != '\n') {
                newline++;
            }
            uint32_t line_length = newline;
            if (line_length > 0 && line[line_length - 1] == '\r') {
                line_length--;
            }
            if (line_length > CONF_LINE_MAX) {
                parse_failed = true;
                break;
            }
            line[line_length] = '\0';

            if (line_length > 0) {
                rows++;
                if (!parse_line(line, line_length, conf)) {
                    parse_failed = true;
                    break;
                }
            }

            position += newline + 1;
        }
    }

    hal_file_close(&file);

    if (read_failed || parse_failed) {
        reset_to_defaults(conf);
    }
    if (parse_failed) {
        toast_show(toast, CONF_ERROR_TEXT, now_ms);
    }
}

const char *conf_key_name(ConfKey key)
{
    return key < CONF_COUNT ? CONF_KEY_NAMES[key] : "";
}

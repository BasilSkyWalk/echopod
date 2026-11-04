#include "sound/equalizer_text.h"

#include <stddef.h>

enum {
    MAX_TOKENS = 16,
    MAX_FILTER_ID = 999,
    TENTHS = 10,
    FILTER_FIELDS = 3,
    NONE = -1
};

typedef struct Token {
    char *text;
    uint32_t length;
} Token;

typedef struct FilterFields {
    int frequency_tenths;
    int gain_tenths;
    int q_tenths;
    uint8_t found;
} FilterFields;

enum {
    FIELD_FREQUENCY = 1,
    FIELD_GAIN = 2,
    FIELD_Q = 4
};

static char lower(char character)
{
    return character >= 'A' && character <= 'Z' ? (char)(character - 'A' + 'a') : character;
}

static bool token_is(const Token *token, const char *word)
{
    uint32_t index = 0;
    while (word[index] != '\0') {
        if (index >= token->length || lower(token->text[index]) != word[index]) {
            return false;
        }
        index++;
    }
    return index == token->length;
}

static bool token_digits(const Token *token, uint32_t *value)
{
    if (token->length == 0) {
        return false;
    }
    uint32_t result = 0;
    for (uint32_t index = 0; index < token->length; index++) {
        char character = token->text[index];
        if (character < '0' || character > '9') {
            return false;
        }
        result = result * 10 + (uint32_t)(character - '0');
        if (result > MAX_FILTER_ID) {
            return false;
        }
    }
    *value = result;
    return true;
}

static bool parse_tenths(const Token *token, int32_t *value)
{
    uint32_t index = 0;
    bool negative = false;
    if (index < token->length && (token->text[index] == '+' || token->text[index] == '-')) {
        negative = token->text[index] == '-';
        index++;
    }
    int64_t whole = 0;
    int digits = 0;
    while (index < token->length && token->text[index] >= '0' && token->text[index] <= '9') {
        whole = whole * 10 + (token->text[index] - '0');
        if (whole > 1000000) {
            return false;
        }
        index++;
        digits++;
    }
    int tenth_digit = 0;
    int hundredth_digit = 0;
    int fraction_digits = 0;
    if (index < token->length && token->text[index] == '.') {
        index++;
        while (index < token->length && token->text[index] >= '0' && token->text[index] <= '9') {
            if (fraction_digits == 0) {
                tenth_digit = token->text[index] - '0';
            } else if (fraction_digits == 1) {
                hundredth_digit = token->text[index] - '0';
            }
            fraction_digits++;
            index++;
        }
        if (fraction_digits == 0) {
            return false;
        }
    }
    if (digits == 0 && fraction_digits == 0) {
        return false;
    }
    if (index != token->length) {
        return false;
    }
    int64_t tenths = whole * TENTHS + tenth_digit + (hundredth_digit >= 5 ? 1 : 0);
    if (negative) {
        tenths = -tenths;
    }
    if (tenths > INT32_MAX || tenths < INT32_MIN) {
        return false;
    }
    *value = (int32_t)tenths;
    return true;
}

static int32_t rounded_hz(int32_t tenths)
{
    return (tenths + TENTHS / 2) / TENTHS;
}

static int tokenize(char *line, Token *tokens)
{
    int count = 0;
    char *cursor = line;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        if (count == MAX_TOKENS) {
            return NONE;
        }
        tokens[count].text = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            cursor++;
        }
        tokens[count].length = (uint32_t)(cursor - tokens[count].text);
        count++;
        if (*cursor != '\0') {
            *cursor++ = '\0';
        }
    }
    return count;
}

static bool normalize_line(char *line, bool *has_colon)
{
    int colons = 0;
    for (char *cursor = line; *cursor != '\0'; cursor++) {
        if (*cursor == '#') {
            *cursor = '\0';
            break;
        }
        if (*cursor == ':') {
            colons++;
            *cursor = ' ';
        }
    }
    if (colons > 1) {
        return false;
    }
    *has_colon = colons == 1;
    return true;
}

static bool filter_field(const Token *token, uint8_t *field, const char **unit)
{
    if (token_is(token, "fc")) {
        *field = FIELD_FREQUENCY;
        *unit = "hz";
        return true;
    }
    if (token_is(token, "gain")) {
        *field = FIELD_GAIN;
        *unit = "db";
        return true;
    }
    if (token_is(token, "q")) {
        *field = FIELD_Q;
        *unit = NULL;
        return true;
    }
    return false;
}

static bool apply_field(FilterFields *fields, uint8_t field, const Token *value, const Token *unit,
                        EqualizerTextError *error, uint32_t line)
{
    if ((fields->found & field) != 0) {
        error->line = line;
        error->problem = EQUALIZER_TEXT_DUPLICATE_FIELD;
        return false;
    }
    int32_t parsed = 0;
    if (!parse_tenths(value, &parsed)) {
        error->line = line;
        error->problem = EQUALIZER_TEXT_VALUE;
        return false;
    }
    if (field == FIELD_FREQUENCY) {
        if (unit != NULL && !token_is(unit, "hz")) {
            error->line = line;
            error->problem = EQUALIZER_TEXT_SYNTAX;
            return false;
        }
        int32_t frequency = rounded_hz(parsed);
        if (frequency <= 0 || frequency > UINT16_MAX ||
            (uint32_t)frequency * 20 >= EQUALIZER_MAX_RATE * 9) {
            error->line = line;
            error->problem = EQUALIZER_TEXT_FREQUENCY;
            return false;
        }
        fields->frequency_tenths = frequency;
    } else if (field == FIELD_GAIN) {
        if (unit != NULL && !token_is(unit, "db")) {
            error->line = line;
            error->problem = EQUALIZER_TEXT_SYNTAX;
            return false;
        }
        if (parsed < EQUALIZER_GAIN_MIN_TENTHS || parsed > EQUALIZER_GAIN_MAX_TENTHS) {
            error->line = line;
            error->problem = EQUALIZER_TEXT_GAIN;
            return false;
        }
        fields->gain_tenths = parsed;
    } else {
        if (unit != NULL) {
            error->line = line;
            error->problem = EQUALIZER_TEXT_SYNTAX;
            return false;
        }
        if (parsed < EQUALIZER_Q_MIN_TENTHS || parsed > EQUALIZER_Q_MAX_TENTHS) {
            error->line = line;
            error->problem = EQUALIZER_TEXT_Q;
            return false;
        }
        fields->q_tenths = parsed;
    }
    fields->found |= field;
    return true;
}

static bool parse_preamp(Token *tokens, int count, EqualizerCurve *curve, bool *seen,
                         EqualizerTextError *error, uint32_t line)
{
    if (*seen) {
        error->line = line;
        error->problem = EQUALIZER_TEXT_DUPLICATE_PREAMP;
        return false;
    }
    if (count < 2 || count > 3) {
        error->line = line;
        error->problem = EQUALIZER_TEXT_SYNTAX;
        return false;
    }
    int32_t tenths = 0;
    if (!parse_tenths(&tokens[1], &tenths)) {
        error->line = line;
        error->problem = EQUALIZER_TEXT_VALUE;
        return false;
    }
    if (count == 3 && !token_is(&tokens[2], "db")) {
        error->line = line;
        error->problem = EQUALIZER_TEXT_SYNTAX;
        return false;
    }
    if (tenths < EQUALIZER_PREAMP_MIN_TENTHS || tenths > EQUALIZER_PREAMP_MAX_TENTHS) {
        error->line = line;
        error->problem = EQUALIZER_TEXT_PREAMP;
        return false;
    }
    curve->preamp_set = 1;
    curve->preamp_tenths = (int16_t)tenths;
    *seen = true;
    return true;
}

static bool parse_filter(Token *tokens, int count, EqualizerCurve *curve, uint16_t *ids,
                         int *filter_count, EqualizerTextError *error, uint32_t line)
{
    if (*filter_count == EQUALIZER_MAX_BANDS) {
        error->line = line;
        error->problem = EQUALIZER_TEXT_TOO_MANY_FILTERS;
        return false;
    }
    if (count < 4) {
        error->line = line;
        error->problem = EQUALIZER_TEXT_SYNTAX;
        return false;
    }
    uint32_t id = 0;
    if (!token_digits(&tokens[1], &id) || id == 0) {
        error->line = line;
        error->problem = EQUALIZER_TEXT_VALUE;
        return false;
    }
    for (int index = 0; index < *filter_count; index++) {
        if (ids[index] == id) {
            error->line = line;
            error->problem = EQUALIZER_TEXT_DUPLICATE_FILTER;
            return false;
        }
    }
    bool enabled = token_is(&tokens[2], "on");
    if (!enabled && !token_is(&tokens[2], "off")) {
        error->line = line;
        error->problem = EQUALIZER_TEXT_SYNTAX;
        return false;
    }
    if (!token_is(&tokens[3], "pk")) {
        error->line = line;
        error->problem = EQUALIZER_TEXT_UNSUPPORTED_FILTER;
        return false;
    }
    FilterFields fields = {.found = 0};
    int index = 4;
    while (index < count) {
        uint8_t field = 0;
        const char *unit_name = NULL;
        if (!filter_field(&tokens[index], &field, &unit_name)) {
            error->line = line;
            error->problem = EQUALIZER_TEXT_SYNTAX;
            return false;
        }
        if (index + 1 >= count) {
            error->line = line;
            error->problem = EQUALIZER_TEXT_SYNTAX;
            return false;
        }
        const Token *unit = NULL;
        if (unit_name != NULL && index + 2 < count && token_is(&tokens[index + 2], unit_name)) {
            unit = &tokens[index + 2];
        }
        if (!apply_field(&fields, field, &tokens[index + 1], unit, error, line)) {
            return false;
        }
        index += unit != NULL ? 3 : 2;
    }
    if (fields.found != (FIELD_FREQUENCY | FIELD_GAIN | FIELD_Q)) {
        error->line = line;
        error->problem = EQUALIZER_TEXT_SYNTAX;
        return false;
    }
    ids[(*filter_count)++] = (uint16_t)id;
    if (enabled) {
        EqualizerBand *band = &curve->bands[curve->band_count++];
        band->frequency_hz = (uint16_t)fields.frequency_tenths;
        band->gain_tenths = (int16_t)fields.gain_tenths;
        band->q_tenths = (uint8_t)fields.q_tenths;
    }
    return true;
}

static bool parse_line(char *line, uint32_t line_number, EqualizerCurve *curve, bool *preamp_seen,
                       uint16_t *ids, int *filter_count, EqualizerTextError *error)
{
    bool has_colon = false;
    if (!normalize_line(line, &has_colon)) {
        error->line = line_number;
        error->problem = EQUALIZER_TEXT_SYNTAX;
        return false;
    }
    Token tokens[MAX_TOKENS];
    int count = tokenize(line, tokens);
    if (count == NONE) {
        error->line = line_number;
        error->problem = EQUALIZER_TEXT_SYNTAX;
        return false;
    }
    if (count == 0) {
        return true;
    }
    if (!has_colon) {
        error->line = line_number;
        error->problem = EQUALIZER_TEXT_SYNTAX;
        return false;
    }
    if (token_is(&tokens[0], "preamp")) {
        return parse_preamp(tokens, count, curve, preamp_seen, error, line_number);
    }
    if (token_is(&tokens[0], "filter")) {
        return parse_filter(tokens, count, curve, ids, filter_count, error, line_number);
    }
    error->line = line_number;
    error->problem = EQUALIZER_TEXT_UNSUPPORTED_DIRECTIVE;
    return false;
}

bool equalizer_text_parse(const char *text, uint32_t length, EqualizerCurve *curve,
                          EqualizerTextError *error)
{
    *error = (EqualizerTextError){.line = 0, .problem = EQUALIZER_TEXT_OK};
    *curve = (EqualizerCurve){0};
    if (length > EQUALIZER_TEXT_MAX_BYTES) {
        error->problem = EQUALIZER_TEXT_TOO_LARGE;
        return false;
    }
    uint32_t start = 0;
    if (length >= 3 && (uint8_t)text[0] == 0xEF && (uint8_t)text[1] == 0xBB &&
        (uint8_t)text[2] == 0xBF) {
        start = 3;
    }
    bool preamp_seen = false;
    uint16_t ids[EQUALIZER_MAX_BANDS] = {0};
    int filter_count = 0;
    uint32_t position = start;
    uint32_t line_number = 1;
    while (position < length) {
        uint32_t end = position;
        while (end < length && text[end] != '\n') {
            end++;
        }
        uint32_t line_length = end - position;
        if (line_length > EQUALIZER_TEXT_MAX_LINE) {
            error->line = line_number;
            error->problem = EQUALIZER_TEXT_LINE_TOO_LONG;
            return false;
        }
        if (line_length > 0 && text[position + line_length - 1] == '\r') {
            line_length--;
        }
        char line[EQUALIZER_TEXT_MAX_LINE + 1];
        for (uint32_t index = 0; index < line_length; index++) {
            line[index] = text[position + index];
        }
        line[line_length] = '\0';
        if (!parse_line(line, line_number, curve, &preamp_seen, ids, &filter_count, error)) {
            return false;
        }
        position = end < length ? end + 1 : length;
        line_number++;
    }
    if (!preamp_seen && filter_count == 0) {
        error->problem = EQUALIZER_TEXT_EMPTY;
        return false;
    }
    return true;
}

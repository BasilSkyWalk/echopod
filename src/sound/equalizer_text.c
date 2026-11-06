#include "sound/equalizer_text.h"

#include <stddef.h>

enum {
    MAX_TOKENS = 16,
    MAX_FILTER_ID = 999,
    MAX_FILTER_LINES = 32,
    TENTHS = 1,
    HUNDREDTHS = 2,
    NONE = -1,
    BOM_BYTES = 3,
    WINDOW_BYTES = BOM_BYTES + EQUALIZER_TEXT_MAX_LINE + 1
};

typedef enum FilterKind {
    KIND_PEAK = EQUALIZER_PEAK,
    KIND_LOW_SHELF = EQUALIZER_LOW_SHELF,
    KIND_HIGH_SHELF = EQUALIZER_HIGH_SHELF,
    KIND_OTHER
} FilterKind;

typedef struct Token {
    char *text;
    uint32_t length;
} Token;

typedef struct FilterFields {
    int frequency_hz;
    int gain_tenths;
    int q_hundredths;
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

static bool parse_scaled(const Token *token, int places, int32_t *value)
{
    uint32_t index = 0;
    bool negative = false;
    if (index < token->length && (token->text[index] == '+' || token->text[index] == '-')) {
        negative = token->text[index] == '-';
        index++;
    }
    int64_t scaled = 0;
    int digits = 0;
    while (index < token->length && token->text[index] >= '0' && token->text[index] <= '9') {
        scaled = scaled * 10 + (token->text[index] - '0');
        if (scaled > 1000000) {
            return false;
        }
        index++;
        digits++;
    }
    int fraction_digits = 0;
    int rounding = 0;
    if (index < token->length && token->text[index] == '.') {
        index++;
        while (index < token->length && token->text[index] >= '0' && token->text[index] <= '9') {
            int digit = token->text[index] - '0';
            if (fraction_digits < places) {
                scaled = scaled * 10 + digit;
            } else if (fraction_digits == places) {
                rounding = digit >= 5 ? 1 : 0;
            }
            fraction_digits++;
            index++;
        }
        if (fraction_digits == 0) {
            return false;
        }
    }
    if ((digits == 0 && fraction_digits == 0) || index != token->length) {
        return false;
    }
    for (int place = fraction_digits; place < places; place++) {
        scaled *= 10;
    }
    scaled += rounding;
    *value = (int32_t)(negative ? -scaled : scaled);
    return true;
}

static bool parse_tenths(const Token *token, int32_t *value)
{
    return parse_scaled(token, TENTHS, value);
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

static bool fail(EqualizerTextError *error, uint32_t line, EqualizerTextProblem problem)
{
    error->line = line;
    error->problem = problem;
    return false;
}

static bool apply_field(FilterFields *fields, uint8_t field, const Token *value, const Token *unit,
                        EqualizerTextError *error, uint32_t line)
{
    if ((fields->found & field) != 0) {
        return fail(error, line, EQUALIZER_TEXT_DUPLICATE_FIELD);
    }
    int places = field == FIELD_FREQUENCY ? 0 : field == FIELD_GAIN ? TENTHS : HUNDREDTHS;
    int32_t parsed = 0;
    if (!parse_scaled(value, places, &parsed)) {
        return fail(error, line, EQUALIZER_TEXT_VALUE);
    }
    if (field == FIELD_FREQUENCY) {
        if (parsed <= 0 || parsed > UINT16_MAX || (uint32_t)parsed * 20 >= EQUALIZER_MAX_RATE * 9) {
            return fail(error, line, EQUALIZER_TEXT_FREQUENCY);
        }
        fields->frequency_hz = parsed;
    } else if (field == FIELD_GAIN) {
        if (parsed < EQUALIZER_GAIN_MIN_TENTHS || parsed > EQUALIZER_GAIN_MAX_TENTHS) {
            return fail(error, line, EQUALIZER_TEXT_GAIN);
        }
        fields->gain_tenths = parsed;
    } else {
        if (unit != NULL) {
            return fail(error, line, EQUALIZER_TEXT_SYNTAX);
        }
        if (parsed < EQUALIZER_Q_MIN_HUNDREDTHS || parsed > EQUALIZER_Q_MAX_HUNDREDTHS) {
            return fail(error, line, EQUALIZER_TEXT_Q);
        }
        fields->q_hundredths = parsed;
    }
    fields->found |= field;
    return true;
}

static FilterKind filter_kind(const Token *token)
{
    if (token_is(token, "pk") || token_is(token, "peq") || token_is(token, "modal")) {
        return KIND_PEAK;
    }
    if (token_is(token, "lsc") || token_is(token, "ls")) {
        return KIND_LOW_SHELF;
    }
    if (token_is(token, "hsc") || token_is(token, "hs")) {
        return KIND_HIGH_SHELF;
    }
    return KIND_OTHER;
}

typedef enum FieldsResult {
    FIELDS_READ,
    FIELDS_OTHER_FORM,
    FIELDS_BAD
} FieldsResult;

static FieldsResult read_fields(Token *tokens, int count, FilterKind kind, FilterFields *fields,
                                EqualizerTextError *error, uint32_t line)
{
    int index = 4;
    while (index < count) {
        uint8_t field = 0;
        const char *unit_name = NULL;
        if (!filter_field(&tokens[index], &field, &unit_name)) {
            if (index == 4 || token_is(&tokens[index], "bw")) {
                return FIELDS_OTHER_FORM;
            }
            fail(error, line, EQUALIZER_TEXT_SYNTAX);
            return FIELDS_BAD;
        }
        if (index + 1 >= count) {
            fail(error, line, EQUALIZER_TEXT_SYNTAX);
            return FIELDS_BAD;
        }
        const Token *unit = NULL;
        if (index + 2 < count && unit_name != NULL && token_is(&tokens[index + 2], unit_name)) {
            unit = &tokens[index + 2];
        }
        if (!apply_field(fields, field, &tokens[index + 1], unit, error, line)) {
            return FIELDS_BAD;
        }
        index += unit != NULL ? 3 : 2;
    }
    if (kind != KIND_PEAK && fields->found == (FIELD_FREQUENCY | FIELD_GAIN)) {
        return FIELDS_OTHER_FORM;
    }
    if (fields->found != (FIELD_FREQUENCY | FIELD_GAIN | FIELD_Q)) {
        fail(error, line, EQUALIZER_TEXT_SYNTAX);
        return FIELDS_BAD;
    }
    return FIELDS_READ;
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
    if (*filter_count == MAX_FILTER_LINES) {
        return fail(error, line, EQUALIZER_TEXT_TOO_MANY_FILTERS);
    }
    if (count < 4) {
        return fail(error, line, EQUALIZER_TEXT_SYNTAX);
    }
    uint32_t id = 0;
    if (!token_digits(&tokens[1], &id) || id == 0) {
        return fail(error, line, EQUALIZER_TEXT_VALUE);
    }
    for (int index = 0; index < *filter_count; index++) {
        if (ids[index] == id) {
            return fail(error, line, EQUALIZER_TEXT_DUPLICATE_FILTER);
        }
    }
    bool enabled = token_is(&tokens[2], "on");
    if (!enabled && !token_is(&tokens[2], "off")) {
        return fail(error, line, EQUALIZER_TEXT_SYNTAX);
    }
    ids[(*filter_count)++] = (uint16_t)id;
    FilterKind kind = filter_kind(&tokens[3]);
    FilterFields fields = {.found = 0};
    FieldsResult result = kind == KIND_OTHER
                              ? FIELDS_OTHER_FORM
                              : read_fields(tokens, count, kind, &fields, error, line);
    if (result == FIELDS_BAD) {
        return false;
    }
    if (!enabled) {
        return true;
    }
    if (result == FIELDS_OTHER_FORM) {
        error->skipped++;
        return true;
    }
    if (curve->band_count == EQUALIZER_MAX_BANDS) {
        return fail(error, line, EQUALIZER_TEXT_TOO_MANY_FILTERS);
    }
    curve->bands[curve->band_count++] =
        (EqualizerBand){.frequency_hz = (uint16_t)fields.frequency_hz,
                        .gain_tenths = (int16_t)fields.gain_tenths,
                        .q_hundredths = (uint16_t)fields.q_hundredths,
                        .filter = (uint8_t)kind};
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

bool equalizer_text_parse_from(EqualizerTextSource source, uint32_t length, EqualizerCurve *curve,
                               EqualizerTextError *error)
{
    *error = (EqualizerTextError){.line = 0, .problem = EQUALIZER_TEXT_OK, .skipped = 0};
    *curve = (EqualizerCurve){0};
    if (length > EQUALIZER_TEXT_MAX_BYTES) {
        error->problem = EQUALIZER_TEXT_TOO_LARGE;
        return false;
    }
    char window[WINDOW_BYTES + 1];
    uint32_t position = 0;
    uint32_t line_number = 1;
    bool preamp_seen = false;
    uint16_t ids[MAX_FILTER_LINES] = {0};
    int filter_count = 0;
    while (position < length) {
        uint32_t wanted = length - position;
        wanted = wanted < WINDOW_BYTES ? wanted : WINDOW_BYTES;
        if (!source.read(source.context, position, window, wanted)) {
            error->problem = EQUALIZER_TEXT_UNREADABLE;
            return false;
        }
        uint32_t start = 0;
        if (position == 0 && wanted >= 3 && (uint8_t)window[0] == 0xEF &&
            (uint8_t)window[1] == 0xBB && (uint8_t)window[2] == 0xBF) {
            start = 3;
        }
        uint32_t end = start;
        while (end < wanted && window[end] != '\n') {
            end++;
        }
        uint32_t line_length = end - start;
        if (line_length > EQUALIZER_TEXT_MAX_LINE ||
            (end == wanted && position + wanted < length)) {
            return fail(error, line_number, EQUALIZER_TEXT_LINE_TOO_LONG);
        }
        if (line_length > 0 && window[start + line_length - 1] == '\r') {
            line_length--;
        }
        window[start + line_length] = '\0';
        if (!parse_line(window + start, line_number, curve, &preamp_seen, ids, &filter_count,
                        error)) {
            return false;
        }
        position += end < wanted ? end + 1 : wanted;
        line_number++;
    }
    if (!preamp_seen && filter_count == 0) {
        error->problem = EQUALIZER_TEXT_EMPTY;
        return false;
    }
    return true;
}

static bool read_memory(void *context, uint32_t offset, char *buffer, uint32_t length)
{
    const char *text = context;
    for (uint32_t index = 0; index < length; index++) {
        buffer[index] = text[offset + index];
    }
    return true;
}

bool equalizer_text_parse(const char *text, uint32_t length, EqualizerCurve *curve,
                          EqualizerTextError *error)
{
    EqualizerTextSource source = {read_memory, (void *)(uintptr_t)text};
    return equalizer_text_parse_from(source, length, curve, error);
}

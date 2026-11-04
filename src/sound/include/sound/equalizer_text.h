#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "dsp/equalizer.h"

enum {
    EQUALIZER_TEXT_MAX_BYTES = 4096,
    EQUALIZER_TEXT_MAX_LINE = 160
};

typedef enum EqualizerTextProblem {
    EQUALIZER_TEXT_OK,
    EQUALIZER_TEXT_TOO_LARGE,
    EQUALIZER_TEXT_UNREADABLE,
    EQUALIZER_TEXT_EMPTY,
    EQUALIZER_TEXT_TOO_MANY_FILTERS,
    EQUALIZER_TEXT_LINE_TOO_LONG,
    EQUALIZER_TEXT_UNSUPPORTED_DIRECTIVE,
    EQUALIZER_TEXT_UNSUPPORTED_FILTER,
    EQUALIZER_TEXT_DUPLICATE_FILTER,
    EQUALIZER_TEXT_DUPLICATE_FIELD,
    EQUALIZER_TEXT_DUPLICATE_PREAMP,
    EQUALIZER_TEXT_SYNTAX,
    EQUALIZER_TEXT_VALUE,
    EQUALIZER_TEXT_FREQUENCY,
    EQUALIZER_TEXT_GAIN,
    EQUALIZER_TEXT_Q,
    EQUALIZER_TEXT_PREAMP
} EqualizerTextProblem;

typedef struct EqualizerTextError {
    uint32_t line;
    EqualizerTextProblem problem;
} EqualizerTextError;

bool equalizer_text_parse(const char *text, uint32_t length, EqualizerCurve *curve,
                          EqualizerTextError *error);

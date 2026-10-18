#pragma once

#include <stdint.h>

enum {
    SORT_FOLD_COUNT = 2434,
    CASE_FOLD_COUNT = 1289
};

typedef struct SortFold {
    uint32_t code_point;
    uint16_t offset;
    uint8_t length;
} SortFold;

extern const SortFold SORT_FOLDS[SORT_FOLD_COUNT];
extern const uint8_t SORT_FOLD_TEXT[];
extern const SortFold CASE_FOLDS[CASE_FOLD_COUNT];
extern const uint8_t CASE_FOLD_TEXT[];

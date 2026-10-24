#pragma once

#include <stdint.h>

enum {
    SORT_FOLD_RUN_COUNT = 994,
    SORT_FOLD_TEXT_COUNT = 174,
    CASE_FOLD_RUN_COUNT = 186,
    CASE_FOLD_TEXT_COUNT = 104,
    FOLD_RUN_DELTA = 0,
    FOLD_RUN_SAME = 1,
    FOLD_RUN_DELETE = 2
};

typedef struct FoldRun {
    uint16_t first;
    uint16_t value;
    uint8_t count;
    uint8_t shape;
} FoldRun;

typedef struct FoldText {
    uint16_t code_point;
    uint16_t offset;
    uint8_t length;
} FoldText;

extern const FoldRun SORT_FOLD_RUNS[SORT_FOLD_RUN_COUNT];
extern const FoldText SORT_FOLD_TEXTS[SORT_FOLD_TEXT_COUNT];
extern const uint8_t SORT_FOLD_BYTES[];
extern const FoldRun CASE_FOLD_RUNS[CASE_FOLD_RUN_COUNT];
extern const FoldText CASE_FOLD_TEXTS[CASE_FOLD_TEXT_COUNT];
extern const uint8_t CASE_FOLD_BYTES[];

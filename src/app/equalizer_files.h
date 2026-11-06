#pragma once

#include "equalizer_store.h"

enum {
    EQUALIZER_FILES_MAX = 8
};

typedef struct EqualizerFileList {
    int count;
    char names[EQUALIZER_FILES_MAX][EQUALIZER_PRESET_NAME_CAPACITY];
} EqualizerFileList;

void equalizer_files_scan(EqualizerFileList *list);
void equalizer_files_name(const EqualizerFileList *list, int index, char *name);
uint32_t equalizer_files_stem_length(const char *name);

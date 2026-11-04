#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sound/equalizer_file.h"
#include "sound/equalizer_text.h"

extern const char EQUALIZER_STORE_PATH[];
extern const char EQUALIZER_FOLDER_PATH[];

typedef struct EqualizerStore {
    EqualizerChoice choice;
    uint8_t saved[EQUALIZER_FILE_SIZE];
    bool has_saved;
} EqualizerStore;

void equalizer_store_load(EqualizerStore *store);
bool equalizer_store_save(EqualizerStore *store);
bool equalizer_store_apply(EqualizerStore *store);
bool equalizer_store_select_file(EqualizerStore *store, const char *name,
                                 EqualizerTextError *error);
bool equalizer_store_file_curve(const char *name, EqualizerCurve *curve, EqualizerTextError *error);

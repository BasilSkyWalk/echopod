#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sound/equalizer_file.h"

extern const char EQUALIZER_STORE_PATH[];

typedef struct EqualizerStore {
    EqualizerChoice choice;
    uint8_t saved[EQUALIZER_FILE_SIZE];
    bool has_saved;
} EqualizerStore;

void equalizer_store_load(EqualizerStore *store);
bool equalizer_store_save(EqualizerStore *store);
void equalizer_store_apply(const EqualizerStore *store);

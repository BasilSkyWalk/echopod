#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "hal/audio.h"

enum {
    DIAGNOSTICS_VALUE_CAPACITY = 24
};

typedef struct DmaToneView {
    bool tried;
    bool stopped;
    HalToneResult result;
    HalDmaToneStatus status;
} DmaToneView;

const char *diagnostics_tone_result_text(HalToneResult result);
void diagnostics_dma_tone_text(DmaToneView view, char *text, size_t capacity);

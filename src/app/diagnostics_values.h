#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "hal/audio.h"
#include "hal/system.h"

enum {
    DIAGNOSTICS_VALUE_CAPACITY = 24
};

typedef struct DmaToneView {
    bool tried;
    bool stopped;
    HalToneResult result;
    HalDmaToneStatus status;
    HalToneDetail detail;
} DmaToneView;

const char *diagnostics_tone_result_text(HalToneResult result);
void diagnostics_tone_text(HalToneResult result, HalToneDetail detail, char *text, size_t capacity);
void diagnostics_dma_tone_text(DmaToneView view, char *text, size_t capacity);
void diagnostics_boot_clocks_text(HalBootClocks clocks, char *text, size_t capacity);

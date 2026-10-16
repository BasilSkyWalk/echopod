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
void diagnostics_boot_pll_text(HalBootClocks clocks, char *text, size_t capacity);
uint32_t diagnostics_next_clock_mhz(uint32_t current_mhz);
void diagnostics_clock_text(uint32_t mhz, bool failed, char *text, size_t capacity);
void diagnostics_loop_text(bool run, HalLoopCycles cycles, char *text, size_t capacity);
void diagnostics_core1_text(bool run, HalCore1Test test, char *text, size_t capacity);
void diagnostics_core1_decode_text(bool run, HalCore1Decode result, char *text, size_t capacity);

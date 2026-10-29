#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "hal/audio.h"
#include "hal/power.h"
#include "hal/system.h"

enum {
    DIAGNOSTICS_VALUE_CAPACITY = 32
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
void diagnostics_usb_registers_text(bool captured, uint32_t first, uint32_t second, char *text,
                                    size_t capacity);
void diagnostics_usb_last_text(HalUsbLast last, char *text, size_t capacity);
uint32_t diagnostics_next_clock_mhz(uint32_t current_mhz);
void diagnostics_clock_text(uint32_t mhz, bool failed, char *text, size_t capacity);
void diagnostics_loop_text(bool run, HalLoopCycles cycles, char *text, size_t capacity);
void diagnostics_battery_text(HalBattery battery, char *text, size_t capacity);
void diagnostics_boot_hold_text(HalPowerHold hold, char *text, size_t capacity);
void diagnostics_jack_pin_text(HalJackPin pin, char *text, size_t capacity);
void diagnostics_start_text(HalStartInfo info, char *text, size_t capacity);
void diagnostics_last_step_text(HalStartInfo info, char *text, size_t capacity);
void diagnostics_last_battery_text(HalStartInfo info, char *text, size_t capacity);
void diagnostics_high_ram_text(bool run, HalHighRamTest test, char *text, size_t capacity);
void diagnostics_core1_text(bool run, HalCore1Test test, char *text, size_t capacity);
void diagnostics_core1_accel_text(bool run, HalCore1Accel result, char *text, size_t capacity);

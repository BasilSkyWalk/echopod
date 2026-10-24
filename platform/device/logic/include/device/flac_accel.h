#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "codec/flac.h"

#define FLAC_ACCEL_BASE 0x01060000u
#define FLAC_ACCEL_DMAC 0x01070000u
#define FLAC_ACCEL_COEFFICIENTS 0x0106B000u

enum {
    FLAC_ACCEL_MAX_JOB = 4088,
    FLAC_ACCEL_MAX_ORDER = 32,
    FLAC_ACCEL_TEST_BLOCK = 4608,
    FLAC_ACCEL_TEST_WORDS = 2 * FLAC_ACCEL_TEST_BLOCK
};

typedef struct AccelBus {
    uint32_t (*read)(void *context, uint32_t address);
    void (*write)(void *context, uint32_t address, uint32_t value);
    void *context;
} AccelBus;

typedef struct AccelTest {
    bool narrow_ok;
    bool wide_ok;
    bool timed_out;
    uint8_t failed_vector;
    uint32_t failed_sample;
} AccelTest;

typedef struct AccelHook {
    const AccelBus *bus;
    const int32_t *window;
    uint32_t window_address;
    bool narrow;
    bool wide;
    uint32_t restored;
    uint32_t timeouts;
} AccelHook;

void flac_accel_start(const AccelBus *bus);
FlacLpcHook flac_accel_hook(AccelHook *hook);
bool flac_accel_restore(const AccelBus *bus, uint32_t address, uint32_t count,
                        const int32_t *coefficients, int order, int shift);
void flac_lpc_restore(int32_t *samples, uint32_t count, const int32_t *coefficients, int order,
                      int shift);
bool flac_lpc_is_wide(int bits, int precision, int order);
typedef struct AccelBlock {
    uint32_t samples;
    int order;
    int shift;
    int32_t coefficients[FLAC_ACCEL_MAX_ORDER];
} AccelBlock;

AccelBlock flac_accel_timing_block(int32_t *scratch);
AccelTest flac_accel_self_test(const AccelBus *bus, int32_t *scratch, uint32_t scratch_address);

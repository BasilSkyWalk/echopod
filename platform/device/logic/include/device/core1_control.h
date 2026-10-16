#pragma once

#include <stdint.h>

#define CORE1_CODE_BASE 0x01000000u
#define CORE1_CODE_BYTES 0x00020000u
#define CORE1_SHARED_BASE 0x01020000u
#define CORE1_SHARED_BYTES 0x00001000u
#define CORE1_SHARED_READY (CORE1_SHARED_BASE + 0x00u)
#define CORE1_SHARED_REGISTER_CENTI (CORE1_SHARED_BASE + 0x04u)
#define CORE1_SHARED_MEMORY_CENTI (CORE1_SHARED_BASE + 0x08u)
#define CORE1_SHARED_COMMAND (CORE1_SHARED_BASE + 0x0Cu)
#define CORE1_SHARED_INPUT_BYTES (CORE1_SHARED_BASE + 0x10u)
#define CORE1_SHARED_SAMPLE_RATE (CORE1_SHARED_BASE + 0x14u)
#define CORE1_SHARED_CHANNELS (CORE1_SHARED_BASE + 0x18u)
#define CORE1_SHARED_BITS (CORE1_SHARED_BASE + 0x1Cu)
#define CORE1_SHARED_MIN_BLOCK (CORE1_SHARED_BASE + 0x20u)
#define CORE1_SHARED_MAX_BLOCK (CORE1_SHARED_BASE + 0x24u)
#define CORE1_SHARED_TOTAL_SAMPLES (CORE1_SHARED_BASE + 0x28u)
#define CORE1_SHARED_BLOCKS (CORE1_SHARED_BASE + 0x30u)
#define CORE1_SHARED_TICKS_TOTAL (CORE1_SHARED_BASE + 0x34u)
#define CORE1_SHARED_TICKS_LONGEST (CORE1_SHARED_BASE + 0x38u)
#define CORE1_SHARED_LAST_RESULT (CORE1_SHARED_BASE + 0x3Cu)
#define CORE1_COMMAND_BENCH 0u
#define CORE1_COMMAND_DECODE 1u
#define CORE1_INPUT_BASE 0x0102E000u
#define CORE1_INPUT_BYTES 0x00032000u
#define CORE1_READY_MAGIC 0x45434831u
#define CORE1_MAILBOX_B2A_COMMAND 0x40110030u
#define CORE1_MAILBOX_B2A_DATA 0x40110034u
#define CORE1_READY_COMMAND 1u

typedef enum Core1Result {
    CORE1_OK,
    CORE1_NO_POWER,
    CORE1_NO_REPLY,
    CORE1_BAD_REPLY,
    CORE1_NO_STOP,
    CORE1_TOO_BIG
} Core1Result;

typedef struct Core1Bus {
    uint32_t (*read)(void *context, uint32_t address);
    void (*write)(void *context, uint32_t address, uint32_t value);
    void (*delay_ms)(void *context, uint32_t milliseconds);
    uint32_t (*now_ms)(void *context);
    void *context;
} Core1Bus;

uint32_t core1_divider_for(uint32_t pll_mhz);
Core1Result core1_power_up(const Core1Bus *bus, uint32_t pll_mhz, uint32_t divider);
Core1Result core1_load(const Core1Bus *bus, const uint8_t *image, uint32_t length);
Core1Result core1_run(const Core1Bus *bus, uint32_t timeout_ms);
Core1Result core1_start(const Core1Bus *bus, const uint8_t *image, uint32_t length);
Core1Result core1_stop(const Core1Bus *bus);

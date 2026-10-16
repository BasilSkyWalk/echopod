#pragma once

#include <stdint.h>

#define CORE1_CODE_BASE 0x01000000u
#define CORE1_CODE_BYTES 0x00020000u
#define CORE1_SHARED_BASE 0x01020000u
#define CORE1_SHARED_BYTES 0x00001000u
#define CORE1_SHARED_READY (CORE1_SHARED_BASE + 0x00u)
#define CORE1_SHARED_REGISTER_CENTI (CORE1_SHARED_BASE + 0x04u)
#define CORE1_SHARED_MEMORY_CENTI (CORE1_SHARED_BASE + 0x08u)
#define CORE1_READY_MAGIC 0x45434831u
#define CORE1_MAILBOX_B2A_COMMAND 0x40110030u
#define CORE1_MAILBOX_B2A_DATA 0x40110034u
#define CORE1_READY_COMMAND 1u

typedef enum Core1Result {
    CORE1_OK,
    CORE1_NO_POWER,
    CORE1_NO_REPLY,
    CORE1_BAD_REPLY,
    CORE1_NO_STOP
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
Core1Result core1_start(const Core1Bus *bus, const uint8_t *image, uint32_t length);
Core1Result core1_stop(const Core1Bus *bus);

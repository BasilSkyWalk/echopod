#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    CARD_WRITE_SINGLE_BLOCK = 24,
    CARD_WRITE_MULTIPLE_BLOCK = 25
};

typedef struct CardWriter {
    bool (*write_data)(void *context, uint8_t command, uint32_t address, uint32_t count,
                       const uint32_t *words);
    bool (*stop)(void *context);
    bool (*wait_ready)(void *context);
    void *context;
} CardWriter;

bool card_write(const CardWriter *writer, uint32_t address, uint32_t count, const uint32_t *words);

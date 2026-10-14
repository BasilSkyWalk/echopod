#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct BlockDevice {
    bool (*read)(void *context, uint32_t lba, uint32_t count, void *buffer);
    void *context;
} BlockDevice;

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct ByteSource {
    bool (*read)(void *context, uint32_t offset, void *buffer, uint32_t length);
    void *context;
} ByteSource;

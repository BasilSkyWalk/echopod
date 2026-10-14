#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct HalFile {
    int32_t handle;
    uint32_t size;
} HalFile;

bool hal_file_open(HalFile *file, const char *path);
bool hal_file_read(HalFile file, uint32_t offset, void *buffer, uint32_t length);
void hal_file_close(HalFile *file);

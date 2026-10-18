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
bool hal_file_write(const char *path, const void *bytes, uint32_t length);

enum {
    HAL_NAME_CAPACITY = 768
};

typedef struct HalDir {
    int handle;
} HalDir;

typedef struct HalDirEntry {
    bool directory;
    bool hidden;
    uint32_t size;
    char name[HAL_NAME_CAPACITY];
} HalDirEntry;

bool hal_dir_open(HalDir *dir, const char *path);
bool hal_dir_next(HalDir *dir, HalDirEntry *entry);
void hal_dir_close(HalDir *dir);

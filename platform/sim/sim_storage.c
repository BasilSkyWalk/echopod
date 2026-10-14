#include "sim_storage.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hal/storage.h"

enum {
    OPEN_FILE_CAPACITY = 8,
    PATH_CAPACITY = 1024
};

static const char SYSTEM_PREFIX[] = "/.echo/";

typedef struct SimStorage {
    const char *card_directory;
    const char *system_directory;
    FILE *files[OPEN_FILE_CAPACITY];
} SimStorage;

static SimStorage storage;

void sim_storage_configure(const char *card_directory, const char *system_directory)
{
    storage.card_directory = card_directory;
    storage.system_directory = system_directory;
}

bool sim_storage_host_path(const char *path, char *buffer, size_t capacity)
{
    size_t prefix_length = sizeof SYSTEM_PREFIX - 1;
    int written = -1;
    if (storage.system_directory != NULL && strncmp(path, SYSTEM_PREFIX, prefix_length) == 0) {
        written =
            snprintf(buffer, capacity, "%s/%s", storage.system_directory, path + prefix_length);
    } else if (storage.card_directory != NULL) {
        written = snprintf(buffer, capacity, "%s%s", storage.card_directory, path);
    }
    return written > 0 && (size_t)written < capacity;
}

static int free_slot(void)
{
    for (int slot = 0; slot < OPEN_FILE_CAPACITY; slot++) {
        if (storage.files[slot] == NULL) {
            return slot;
        }
    }
    return -1;
}

static long file_size(FILE *handle)
{
    if (fseek(handle, 0, SEEK_END) != 0) {
        return -1;
    }
    long size = ftell(handle);
    return fseek(handle, 0, SEEK_SET) == 0 ? size : -1;
}

static FILE *file_of(HalFile file)
{
    if (file.handle < 0 || file.handle >= OPEN_FILE_CAPACITY) {
        return NULL;
    }
    return storage.files[file.handle];
}

bool hal_file_open(HalFile *file, const char *path)
{
    char resolved[PATH_CAPACITY];
    int slot = free_slot();
    *file = (HalFile){.handle = -1, .size = 0};
    if (slot < 0 || !sim_storage_host_path(path, resolved, sizeof resolved)) {
        return false;
    }
    FILE *handle = fopen(resolved, "rb");
    if (handle == NULL) {
        return false;
    }
    long size = file_size(handle);
    if (size < 0 || (unsigned long)size > UINT32_MAX) {
        fclose(handle);
        return false;
    }
    storage.files[slot] = handle;
    *file = (HalFile){.handle = slot, .size = (uint32_t)size};
    return true;
}

bool hal_file_read(HalFile file, uint32_t offset, void *buffer, uint32_t length)
{
    FILE *handle = file_of(file);
    if (handle == NULL || fseek(handle, (long)offset, SEEK_SET) != 0) {
        return false;
    }
    return fread(buffer, 1, length, handle) == length;
}

void hal_file_close(HalFile *file)
{
    FILE *handle = file_of(*file);
    if (handle != NULL) {
        fclose(handle);
        storage.files[file->handle] = NULL;
    }
    file->handle = -1;
}

static bool is_system_path(const char *path)
{
    return storage.system_directory != NULL &&
           strncmp(path, SYSTEM_PREFIX, sizeof SYSTEM_PREFIX - 1) == 0;
}

bool hal_file_write(const char *path, const void *bytes, uint32_t length)
{
    char resolved[PATH_CAPACITY];
    char temporary[PATH_CAPACITY];
    if (!is_system_path(path) || !sim_storage_host_path(path, resolved, sizeof resolved)) {
        return false;
    }
    int written_length = snprintf(temporary, sizeof temporary, "%s.tmp", resolved);
    if (written_length <= 0 || (size_t)written_length >= sizeof temporary) {
        return false;
    }
    FILE *handle = fopen(temporary, "wb");
    if (handle == NULL) {
        return false;
    }
    bool written = fwrite(bytes, 1, length, handle) == length;
    written = fclose(handle) == 0 && written;
    return written && rename(temporary, resolved) == 0;
}

void sim_storage_close_all(void)
{
    for (int slot = 0; slot < OPEN_FILE_CAPACITY; slot++) {
        if (storage.files[slot] != NULL) {
            fclose(storage.files[slot]);
            storage.files[slot] = NULL;
        }
    }
}

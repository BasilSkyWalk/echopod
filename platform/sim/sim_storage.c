#include "sim_storage.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "hal/storage.h"

enum {
    OPEN_FILE_CAPACITY = 8,
    OPEN_DIR_CAPACITY = 16,
    PATH_CAPACITY = 1024
};

static const char SYSTEM_PREFIX[] = "/.echo/";

typedef struct SimStorage {
    const char *card_directory;
    const char *system_directory;
    FILE *files[OPEN_FILE_CAPACITY];
    DIR *dirs[OPEN_DIR_CAPACITY];
    char dir_paths[OPEN_DIR_CAPACITY][PATH_CAPACITY];
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
    setvbuf(handle, NULL, _IONBF, 0);
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

static bool replace_file(const char *from, const char *to)
{
#if defined(_WIN32)
    (void)remove(to);
#endif
    return rename(from, to) == 0;
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
    return written && replace_file(temporary, resolved);
}

bool hal_file_write_at(const char *path, uint32_t offset, const void *bytes, uint32_t length)
{
    char resolved[PATH_CAPACITY];
    if (!is_system_path(path) || !sim_storage_host_path(path, resolved, sizeof resolved)) {
        return false;
    }
    FILE *handle = fopen(resolved, "r+b");
    if (handle == NULL) {
        return false;
    }
    bool written =
        fseek(handle, (long)offset, SEEK_SET) == 0 && fwrite(bytes, 1, length, handle) == length;
    return fclose(handle) == 0 && written;
}

HalReserveResult hal_file_reserve(const char *path, uint32_t size)
{
    char resolved[PATH_CAPACITY];
    struct stat existing;
    if (size == 0 || !is_system_path(path) ||
        !sim_storage_host_path(path, resolved, sizeof resolved)) {
        return HAL_RESERVE_FAILED;
    }
    if (stat(resolved, &existing) == 0) {
        return HAL_RESERVE_EXISTS;
    }
    FILE *handle = fopen(resolved, "wb");
    if (handle == NULL) {
        return HAL_RESERVE_FAILED;
    }
    bool sized = ftruncate(fileno(handle), (off_t)size) == 0;
    sized = fclose(handle) == 0 && sized;
    return sized ? HAL_RESERVE_CREATED : HAL_RESERVE_FAILED;
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

static int free_dir_slot(void)
{
    for (int slot = 0; slot < OPEN_DIR_CAPACITY; slot++) {
        if (storage.dirs[slot] == NULL) {
            return slot;
        }
    }
    return -1;
}

bool hal_dir_open(HalDir *dir, const char *path)
{
    dir->handle = -1;
    int slot = free_dir_slot();
    char *resolved = slot >= 0 ? storage.dir_paths[slot] : NULL;
    if (slot < 0 || !sim_storage_host_path(path, resolved, PATH_CAPACITY)) {
        return false;
    }
    storage.dirs[slot] = opendir(resolved);
    if (storage.dirs[slot] == NULL) {
        return false;
    }
    dir->handle = slot;
    return true;
}

static bool is_dot_name(const char *name)
{
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

static bool describe(int slot, const char *name, HalDirEntry *entry)
{
    char full[PATH_CAPACITY];
    struct stat status;
    int written = snprintf(full, sizeof full, "%s/%s", storage.dir_paths[slot], name);
    if (written <= 0 || (size_t)written >= sizeof full || stat(full, &status) != 0 ||
        strlen(name) >= sizeof entry->name) {
        return false;
    }
    entry->directory = S_ISDIR(status.st_mode);
    entry->hidden = name[0] == '.';
    entry->size =
        entry->directory || (uint64_t)status.st_size > UINT32_MAX ? 0 : (uint32_t)status.st_size;
    entry->modified = (uint32_t)status.st_mtime;
    strcpy(entry->name, name);
    return true;
}

bool hal_dir_next(HalDir *dir, HalDirEntry *entry)
{
    if (dir->handle < 0 || dir->handle >= OPEN_DIR_CAPACITY || storage.dirs[dir->handle] == NULL) {
        return false;
    }
    struct dirent *found = NULL;
    while ((found = readdir(storage.dirs[dir->handle])) != NULL) {
        if (!is_dot_name(found->d_name) && describe(dir->handle, found->d_name, entry)) {
            return true;
        }
    }
    return false;
}

void hal_dir_close(HalDir *dir)
{
    if (dir->handle >= 0 && dir->handle < OPEN_DIR_CAPACITY && storage.dirs[dir->handle] != NULL) {
        closedir(storage.dirs[dir->handle]);
        storage.dirs[dir->handle] = NULL;
    }
    dir->handle = -1;
}

#include <stddef.h>

#include "device/fat.h"
#include "hal/storage.h"
#include "sd.h"

enum {
    OPEN_FILE_CAPACITY = 8
};

typedef enum MountState {
    MOUNT_NOT_TRIED,
    MOUNT_READY,
    MOUNT_FAILED
} MountState;

static MountState mount_state;
static FatVolume volume;
static FatFile files[OPEN_FILE_CAPACITY];
static bool in_use[OPEN_FILE_CAPACITY];

static bool mounted(void)
{
    if (mount_state == MOUNT_NOT_TRIED) {
        BlockDevice device;
        bool ready = sd_start(&device) && fat_mount(&volume, device);
        mount_state = ready ? MOUNT_READY : MOUNT_FAILED;
    }
    return mount_state == MOUNT_READY;
}

static int free_slot(void)
{
    for (int slot = 0; slot < OPEN_FILE_CAPACITY; slot++) {
        if (!in_use[slot]) {
            return slot;
        }
    }
    return -1;
}

static FatFile *file_of(HalFile file)
{
    if (file.handle < 0 || file.handle >= OPEN_FILE_CAPACITY || !in_use[file.handle]) {
        return NULL;
    }
    return &files[file.handle];
}

bool hal_file_open(HalFile *file, const char *path)
{
    *file = (HalFile){.handle = -1, .size = 0};
    int slot = free_slot();
    if (slot < 0 || !mounted() || !fat_open(&volume, path, &files[slot])) {
        return false;
    }
    in_use[slot] = true;
    *file = (HalFile){.handle = slot, .size = files[slot].size};
    return true;
}

bool hal_file_read(HalFile file, uint32_t offset, void *buffer, uint32_t length)
{
    FatFile *open = file_of(file);
    return open != NULL && fat_read(&volume, open, offset, buffer, length);
}

void hal_file_close(HalFile *file)
{
    if (file_of(*file) != NULL) {
        in_use[file->handle] = false;
    }
    file->handle = -1;
}

bool hal_file_write(const char *path, const void *bytes, uint32_t length)
{
    (void)path;
    (void)bytes;
    (void)length;
    return false;
}

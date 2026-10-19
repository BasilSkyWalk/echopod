#include <stddef.h>

#include "board.h"
#include "device/fat.h"
#include "hal/storage.h"
#include "sd.h"
#include "storage_reset.h"

enum {
    OPEN_FILE_CAPACITY = 8,
    OPEN_DIR_CAPACITY = 16
};

typedef enum MountState {
    MOUNT_NOT_TRIED,
    MOUNT_READY,
    MOUNT_FAILED
} MountState;

_Static_assert((int)HAL_NAME_CAPACITY == (int)FAT_NAME_CAPACITY, "listed names fit the HAL entry");

static MountState mount_state;
static FatVolume volume;
static FatFile files[OPEN_FILE_CAPACITY];
static bool in_use[OPEN_FILE_CAPACITY];
static FatDirCursor dirs[OPEN_DIR_CAPACITY];
static bool dir_in_use[OPEN_DIR_CAPACITY];
static FatDirEntry listed;

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
    FatFile file;
    if (!mounted() || !fat_open(&volume, path, &file)) {
        return false;
    }
    board_start_record_step(HAL_START_STEP_CARD_WRITE);
    bool written = fat_write(&volume, &file, 0, bytes, length);
    board_start_record_step_done();
    return written;
}

bool hal_file_write_at(const char *path, uint32_t offset, const void *bytes, uint32_t length)
{
    FatFile file;
    if (!mounted() || !fat_open(&volume, path, &file)) {
        return false;
    }
    board_start_record_step(HAL_START_STEP_CARD_WRITE);
    bool written = fat_write(&volume, &file, offset, bytes, length);
    board_start_record_step_done();
    return written;
}

static BlockDevice card_device;

static bool read_and_beat(void *context, uint32_t lba, uint32_t count, void *buffer)
{
    board_stall_beat();
    return card_device.read(context, lba, count, buffer);
}

static HalReserveResult reserve_result(FatReserveResult result)
{
    switch (result) {
    case FAT_RESERVE_EXISTS:
        return HAL_RESERVE_EXISTS;
    case FAT_RESERVE_CREATED:
        return HAL_RESERVE_CREATED;
    case FAT_RESERVE_NO_SPACE:
        return HAL_RESERVE_NO_SPACE;
    case FAT_RESERVE_FAILED:
        return HAL_RESERVE_FAILED;
    }
    return HAL_RESERVE_FAILED;
}

HalReserveResult hal_file_reserve(const char *path, uint32_t size)
{
    if (!mounted()) {
        return HAL_RESERVE_FAILED;
    }
    card_device = volume.device;
    volume.device.read = read_and_beat;
    board_start_record_step(HAL_START_STEP_CARD_WRITE);
    FatReserveResult result = fat_reserve(&volume, path, size);
    board_start_record_step_done();
    volume.device = card_device;
    return reserve_result(result);
}

void storage_reset(void)
{
    mount_state = MOUNT_NOT_TRIED;
    for (int slot = 0; slot < OPEN_FILE_CAPACITY; slot++) {
        in_use[slot] = false;
    }
    for (int slot = 0; slot < OPEN_DIR_CAPACITY; slot++) {
        dir_in_use[slot] = false;
    }
}

static int free_dir_slot(void)
{
    for (int slot = 0; slot < OPEN_DIR_CAPACITY; slot++) {
        if (!dir_in_use[slot]) {
            return slot;
        }
    }
    return -1;
}

bool hal_dir_open(HalDir *dir, const char *path)
{
    dir->handle = -1;
    int slot = free_dir_slot();
    if (slot < 0 || !mounted() || !fat_open_directory(&volume, path, &dirs[slot])) {
        return false;
    }
    dir_in_use[slot] = true;
    dir->handle = slot;
    return true;
}

bool hal_dir_next(HalDir *dir, HalDirEntry *entry)
{
    if (dir->handle < 0 || dir->handle >= OPEN_DIR_CAPACITY || !dir_in_use[dir->handle] ||
        !fat_next_entry(&volume, &dirs[dir->handle], &listed)) {
        return false;
    }
    entry->directory = listed.file.directory;
    entry->hidden = listed.hidden;
    entry->size = listed.file.directory ? 0 : listed.file.size;
    for (size_t index = 0; index < sizeof entry->name; index++) {
        entry->name[index] = listed.name[index];
    }
    return true;
}

void hal_dir_close(HalDir *dir)
{
    if (dir->handle >= 0 && dir->handle < OPEN_DIR_CAPACITY) {
        dir_in_use[dir->handle] = false;
    }
    dir->handle = -1;
}

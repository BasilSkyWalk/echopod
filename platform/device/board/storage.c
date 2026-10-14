#include "hal/storage.h"

bool hal_file_open(HalFile *file, const char *path)
{
    (void)path;
    *file = (HalFile){.handle = -1, .size = 0};
    return false;
}

bool hal_file_read(HalFile file, uint32_t offset, void *buffer, uint32_t length)
{
    (void)file;
    (void)offset;
    (void)buffer;
    (void)length;
    return false;
}

void hal_file_close(HalFile *file)
{
    file->handle = -1;
}

bool hal_file_write(const char *path, const void *bytes, uint32_t length)
{
    (void)path;
    (void)bytes;
    (void)length;
    return false;
}

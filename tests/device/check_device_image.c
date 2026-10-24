#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device/updater.h"

enum {
    SKIPPED = 77,
    SLOT_SECTORS = 0x11000
};

static unsigned char *image;
static long image_size;

static bool read_image(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    (void)context;
    if ((long)offset > image_size || (long)length > image_size - (long)offset) {
        return false;
    }
    memcpy(buffer, image + offset, length);
    return true;
}

static bool read_blank(void *context, uint32_t lba, uint32_t count, void *buffer)
{
    (void)context;
    (void)lba;
    memset(buffer, 0, (size_t)count * 512);
    return true;
}

static bool refuse_write(void *context, uint32_t lba, uint32_t count, const void *buffer)
{
    (void)context;
    (void)lba;
    (void)count;
    (void)buffer;
    return false;
}

static bool load(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    fseek(file, 0, SEEK_END);
    image_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    image = malloc((size_t)image_size);
    bool ok = image != NULL && fread(image, 1, (size_t)image_size, file) == (size_t)image_size;
    fclose(file);
    return ok;
}

int main(int argument_count, char **arguments)
{
    if (argument_count != 2 || !load(arguments[1])) {
        printf("no device image at %s: skipped\n", argument_count == 2 ? arguments[1] : "?");
        return SKIPPED;
    }
    UpdateFile file = {read_image, (uint32_t)image_size, NULL};
    UpdateStorage storage = {read_blank, refuse_write,          NULL,
                             0x2000,     0x2000 + SLOT_SECTORS, SLOT_SECTORS};
    UpdateCheck check = updater_check(&file, &storage);
    free(image);
    printf("updater check: %s\n", check == UPDATE_VALID ? "valid" : "INVALID");
    return check == UPDATE_VALID ? 0 : 1;
}

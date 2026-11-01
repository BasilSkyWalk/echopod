#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal/language.h"

enum {
    PATH_CAPACITY = 1024,
    PACK_LIMIT = 0x3000
};

static const char PACK_PATHS[] = ECHO_LANGUAGE_PACK_PATHS;
static uint8_t pack[PACK_LIMIT];

static bool path_for(uint32_t language, char *path)
{
    const char *cursor = PACK_PATHS;
    for (uint32_t index = 0; *cursor != '\0'; index++) {
        size_t length = strcspn(cursor, "|");
        if (index == language && length < PATH_CAPACITY) {
            memcpy(path, cursor, length);
            path[length] = '\0';
            return true;
        }
        cursor += length + (cursor[length] == '|' ? 1u : 0u);
    }
    return false;
}

bool hal_language_pack(uint32_t language, const uint8_t **bytes, uint32_t *size)
{
    char path[PATH_CAPACITY];
    if (!path_for(language, path)) {
        return false;
    }
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    size_t read = fread(pack, 1, sizeof pack, file);
    fclose(file);
    if (read == 0) {
        return false;
    }
    *bytes = pack;
    *size = (uint32_t)read;
    return true;
}

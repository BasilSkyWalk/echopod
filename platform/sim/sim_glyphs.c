#include "sim_glyphs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/font_pack.h"

enum {
    PATH_CAPACITY = 1024
};

typedef struct FaceFile {
    uint8_t *bytes;
    uint32_t size;
} FaceFile;

static const char FACE_PATHS[] = ECHO_GLYPH_FACE_PATHS;
static FaceFile faces[FONT_PACK_FACES];
static uint32_t cache[SIM_GLYPHS_CACHE_BYTES / 4];
static uint32_t loaded;

static bool load(const char *path, FaceFile *face)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    long size = fseek(file, 0, SEEK_END) == 0 ? ftell(file) : -1;
    uint8_t *bytes = size > 0 ? malloc((size_t)size) : NULL;
    bool read = bytes != NULL && fseek(file, 0, SEEK_SET) == 0 &&
                fread(bytes, 1, (size_t)size, file) == (size_t)size;
    fclose(file);
    if (!read) {
        free(bytes);
        return false;
    }
    *face = (FaceFile){bytes, (uint32_t)size};
    return true;
}

static bool read_face(void *context, uint32_t face, uint32_t offset, void *buffer, uint32_t length)
{
    (void)context;
    if (face >= FONT_PACK_FACES || offset > faces[face].size ||
        length > faces[face].size - offset) {
        return false;
    }
    memcpy(buffer, faces[face].bytes + offset, length);
    return true;
}

static void load_all(void)
{
    const char *cursor = FACE_PATHS;
    for (uint32_t face = 0; face < FONT_PACK_FACES && *cursor != '\0'; face++) {
        char path[PATH_CAPACITY];
        size_t length = strcspn(cursor, "|");
        if (length < sizeof path && faces[face].bytes == NULL) {
            memcpy(path, cursor, length);
            path[length] = '\0';
            loaded += load(path, &faces[face]) ? 1u : 0u;
        }
        cursor += length + (cursor[length] == '|' ? 1u : 0u);
    }
}

uint32_t sim_glyphs_attach(void)
{
    if (loaded == 0) {
        load_all();
    }
    return font_pack_attach((FontPackReader){read_face, NULL}, cache, sizeof cache);
}

uint32_t sim_glyphs_face_count(void)
{
    return loaded;
}

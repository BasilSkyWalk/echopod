#include "big_library.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "library/library_builder.h"

enum {
    BUILDER_MEMORY = 360 * 1024,
    SCRATCH_SIZE = 32 * 1024 * 1024,
    OUTPUT_CAPACITY = 16 * 1024 * 1024,
    TEXT_CAPACITY = 64,
    MINUTE_MS = 60000
};

typedef struct Buffer {
    uint8_t *bytes;
    uint32_t size;
    uint32_t length;
} Buffer;

static bool buffer_read(void *context, uint32_t offset, void *bytes, uint32_t length)
{
    Buffer *buffer = context;
    if (offset > buffer->size || length > buffer->size - offset) {
        return false;
    }
    memcpy(bytes, buffer->bytes + offset, length);
    return true;
}

static bool buffer_write(void *context, uint32_t offset, const void *bytes, uint32_t length)
{
    Buffer *buffer = context;
    if (offset > buffer->size || length > buffer->size - offset) {
        return false;
    }
    memcpy(buffer->bytes + offset, bytes, length);
    buffer->length = offset + length > buffer->length ? offset + length : buffer->length;
    return true;
}

static void fail(const char *what)
{
    fprintf(stderr, "big library: %s\n", what);
    exit(1);
}

static void add_tracks(LibraryBuilder *builder, uint32_t tracks, uint32_t tracks_per_album)
{
    for (uint32_t index = 0; index < tracks; index++) {
        uint32_t album = index / tracks_per_album;
        char path[TEXT_CAPACITY];
        char title[TEXT_CAPACITY];
        char artist[TEXT_CAPACITY];
        char album_name[TEXT_CAPACITY];
        snprintf(path, sizeof path, "Artist %05u/Album %05u/%05u.mp3", album, album, index);
        snprintf(title, sizeof title, "Song %05u", index);
        snprintf(artist, sizeof artist, "Artist %05u", album);
        snprintf(album_name, sizeof album_name, "Album %05u", album);
        BuilderTrack track = {.path = path,
                              .title = title,
                              .artist = artist,
                              .album_artist = artist,
                              .album = album_name,
                              .genre = "Rock",
                              .year = 2000,
                              .disc = 1,
                              .number = (uint8_t)(index % tracks_per_album + 1),
                              .duration_ms = MINUTE_MS};
        if (library_builder_add_track(builder, &track) != BUILDER_OK) {
            fail("add track");
        }
    }
}

Library *big_library_open(BigLibrary *big, uint32_t tracks, uint32_t tracks_per_album)
{
    uint8_t *memory = malloc(BUILDER_MEMORY);
    Buffer scratch = {.bytes = malloc(SCRATCH_SIZE), .size = SCRATCH_SIZE, .length = 0};
    Buffer output = {.bytes = malloc(OUTPUT_CAPACITY), .size = OUTPUT_CAPACITY, .length = 0};
    if (memory == NULL || scratch.bytes == NULL || output.bytes == NULL) {
        fail("out of host memory");
    }
    LibraryBuilder builder;
    library_builder_init(&builder, memory, BUILDER_MEMORY,
                         (BuilderScratch){buffer_read, buffer_write, &scratch, SCRATCH_SIZE});
    add_tracks(&builder, tracks, tracks_per_album);
    uint32_t length = 0;
    if (library_builder_prepare(&builder) != BUILDER_OK ||
        library_builder_finish(&builder, (ByteSink){buffer_write, &output}, 0, &length) !=
            BUILDER_OK) {
        fail("build");
    }
    free(memory);
    free(scratch.bytes);
    big->bytes = output.bytes;
    big->size = length;
    big->memory = (MemorySource){.bytes = big->bytes, .size = big->size};
    if (library_open(&big->library, memory_source(&big->memory), big->size) != LIBRARY_OK) {
        fail("open");
    }
    return &big->library;
}

void big_library_close(BigLibrary *big)
{
    free(big->bytes);
    big->bytes = NULL;
}

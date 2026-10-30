#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct BuilderTrack {
    const char *path;
    const char *title;
    const char *artist;
    const char *album_artist;
    const char *album;
    const char *genre;
    uint16_t year;
    uint8_t disc;
    uint8_t number;
    uint32_t duration_ms;
    bool album_artist_guessed;
} BuilderTrack;

typedef struct ByteSink {
    bool (*write)(void *context, uint32_t offset, const void *bytes, uint32_t length);
    void *context;
} ByteSink;

typedef struct BuilderScratch {
    bool (*read)(void *context, uint32_t offset, void *bytes, uint32_t length);
    bool (*write)(void *context, uint32_t offset, const void *bytes, uint32_t length);
    void *context;
    uint32_t size;
} BuilderScratch;

typedef enum BuilderResult {
    BUILDER_OK,
    BUILDER_OUT_OF_MEMORY,
    BUILDER_TOO_MANY,
    BUILDER_WRITE_FAILED,
    BUILDER_NO_ROOM
} BuilderResult;

typedef struct LibraryBuilder {
    void *state;
} LibraryBuilder;

void library_builder_init(LibraryBuilder *builder, void *memory, uint32_t size,
                          BuilderScratch scratch);
BuilderResult library_builder_add_track(LibraryBuilder *builder, const BuilderTrack *track);
BuilderResult library_builder_prepare(LibraryBuilder *builder);
BuilderResult library_builder_begin_playlist(LibraryBuilder *builder, const char *name,
                                             const char *folder);
BuilderResult library_builder_playlist_entry(LibraryBuilder *builder, const char *entry);
BuilderResult library_builder_finish(LibraryBuilder *builder, ByteSink sink, uint32_t capacity,
                                     uint32_t *length);

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct ReferenceTrack {
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
} ReferenceTrack;

typedef struct ReferenceSink {
    bool (*write)(void *context, uint32_t offset, const void *bytes, uint32_t length);
    void *context;
} ReferenceSink;

typedef enum ReferenceResult {
    REFERENCE_OK,
    REFERENCE_OUT_OF_MEMORY,
    REFERENCE_TOO_MANY,
    REFERENCE_WRITE_FAILED,
    REFERENCE_NO_ROOM
} ReferenceResult;

typedef struct ReferenceBuilder {
    uint8_t *memory;
    uint32_t size;
    uint32_t slot_count;
    uint32_t string_count;
    uint32_t folder_capacity;
    uint32_t folder_count;
    uint32_t track_count;
    uint32_t tracks_offset;
    uint32_t strings_floor;
    uint32_t empty_string;
    bool prepared;
    ReferenceResult prepare_result;
    uint32_t work_start;
    uint32_t work_used;
    uint32_t work_top;
    void *model;
} ReferenceBuilder;

void reference_builder_init(ReferenceBuilder *builder, void *memory, uint32_t size);
ReferenceResult reference_builder_add_track(ReferenceBuilder *builder, const ReferenceTrack *track);
ReferenceResult reference_builder_prepare(ReferenceBuilder *builder);
ReferenceResult reference_builder_begin_playlist(ReferenceBuilder *builder, const char *name,
                                                 const char *folder);
ReferenceResult reference_builder_playlist_entry(ReferenceBuilder *builder, const char *entry);
ReferenceResult reference_builder_finish(ReferenceBuilder *builder, ReferenceSink sink,
                                         uint32_t capacity, uint32_t *length);

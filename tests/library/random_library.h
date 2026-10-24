#pragma once

#include <stdint.h>

#include "library/library_builder.h"

typedef struct RandomPlaylist {
    char *name;
    char *folder;
    char **entries;
    uint32_t count;
} RandomPlaylist;

typedef struct RandomLibrary {
    BuilderTrack *tracks;
    uint32_t track_count;
    RandomPlaylist *playlists;
    uint32_t playlist_count;
    char **strings;
    uint32_t string_count;
    uint32_t string_capacity;
} RandomLibrary;

void random_library_make(RandomLibrary *library, uint32_t tracks, uint32_t playlists,
                         uint32_t seed);
void random_library_free(RandomLibrary *library);

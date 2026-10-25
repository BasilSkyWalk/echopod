#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "library/library.h"

typedef enum SpokenKind {
    SPOKEN_NONE,
    SPOKEN_AUDIOBOOK,
    SPOKEN_PODCAST
} SpokenKind;

enum {
    SPOKEN_JUMP_FORWARD_MS = 30000,
    SPOKEN_JUMP_BACK_MS = 15000,
    SPOKEN_FINISH_MARGIN_MS = 30000,
    SPOKEN_FINISH_DIVISOR = 10
};

SpokenKind spoken_kind_of(const char *path);
uint32_t spoken_track_key(const char *path);
uint32_t spoken_bookmark_key(const char *path);
bool spoken_near_end(uint32_t position_ms, uint32_t duration_ms);
bool spoken_shelf_folder(const Library *library, SpokenKind kind, uint16_t *folder);
bool spoken_folder_key(const Library *library, uint16_t folder, uint32_t *key);
bool spoken_chapter_of(const Library *library, uint16_t track, uint16_t *chapter);
bool spoken_chapter_track(const Library *library, uint16_t folder, uint16_t chapter,
                          uint16_t *track);

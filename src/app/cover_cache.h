#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/image.h"
#include "hal/storage.h"
#include "library/covers.h"
#include "library/library.h"
#include "ui/cover_flow.h"

enum {
    COVER_CACHE_SLOTS = COVER_FLOW_MAX_VISIBLE
};

typedef enum CoverState {
    COVER_EMPTY,
    COVER_LOADED,
    COVER_MISSING
} CoverState;

typedef struct CoverSlot {
    int position;
    uint16_t album;
    CoverState state;
    Color pixels[COVER_HALF_PIXELS];
} CoverSlot;

typedef struct CoverCache {
    const Library *library;
    HalFile file;
    Covers covers;
    bool available;
    CoverSlot halves[COVER_CACHE_SLOTS];
    uint16_t full_album;
    CoverState full_state;
    Color full[COVER_FULL_PIXELS];
} CoverCache;

extern const char COVER_CACHE_PATH[];

void cover_cache_open(CoverCache *cache, const Library *library);
CoverState cover_cache_half(const CoverCache *cache, int position, uint16_t album, Image *image);
CoverState cover_cache_full(const CoverCache *cache, uint16_t album, Image *image);
bool cover_cache_load_half(CoverCache *cache, int position, uint16_t album);
bool cover_cache_load_full(CoverCache *cache, uint16_t album);

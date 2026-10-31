#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "cover_cache.h"
#include "gfx/canvas.h"
#include "library_store.h"
#include "player.h"
#include "ui/theme.h"

enum {
    HOME_ART_WIDTH = 144,
    HOME_ART_MIN_HEIGHT = 128,
    HOME_ART_CYCLE_MS = 5000
};

typedef struct HomeArt {
    CoverCache *covers;
    const LibraryStore *store;
    Player *player;
    const Theme *theme;
    bool cycling;
    bool has_album;
    uint16_t album;
    uint32_t next_change_ms;
} HomeArt;

void home_art_init(HomeArt *art, CoverCache *covers, const LibraryStore *store, Player *player,
                   const Theme *theme);
void home_art_update(void *context, uint32_t now_ms);
void home_art_draw(void *context, Canvas *canvas, Rect area);

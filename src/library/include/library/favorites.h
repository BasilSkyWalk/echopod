#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "library/library.h"

enum {
    FAVORITES_CAPACITY = 256,
    FAVORITES_FILE_CAPACITY = 12 + FAVORITES_CAPACITY * 4
};

typedef struct Favorites {
    uint32_t keys[FAVORITES_CAPACITY];
    uint16_t count;
} Favorites;

void favorites_clear(Favorites *favorites);
bool favorites_contains(const Favorites *favorites, uint32_t key);
bool favorites_toggle(Favorites *favorites, uint32_t key);
uint32_t favorites_encode(const Favorites *favorites, uint8_t *bytes);
bool favorites_decode(const uint8_t *bytes, uint32_t length, Favorites *favorites);
uint16_t favorites_collect_albums(const Favorites *favorites, const Library *library,
                                  uint16_t *albums, uint16_t capacity);

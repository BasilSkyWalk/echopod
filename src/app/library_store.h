#pragma once

#include <stdbool.h>

#include "hal/storage.h"
#include "library/block_cache.h"
#include "library/library.h"

typedef struct LibraryStore {
    HalFile file;
    BlockCache cache;
    Library library;
    bool available;
} LibraryStore;

extern const char LIBRARY_STORE_PATH[];

bool library_store_open(LibraryStore *store);

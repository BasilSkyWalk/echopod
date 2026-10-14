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

bool library_store_open(LibraryStore *store);

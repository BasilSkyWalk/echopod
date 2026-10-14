#include "library_store.h"

static const char LIBRARY_PATH[] = "/.echo/library.db";

static bool read_file(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    const LibraryStore *store = context;
    return hal_file_read(store->file, offset, buffer, length);
}

bool library_store_open(LibraryStore *store)
{
    store->available = false;
    if (!hal_file_open(&store->file, LIBRARY_PATH)) {
        return false;
    }
    ByteSource file_source = {.read = read_file, .context = store};
    block_cache_init(&store->cache, file_source, store->file.size);
    LibraryStatus status =
        library_open(&store->library, block_cache_source(&store->cache), store->file.size);
    store->available = status == LIBRARY_OK;
    if (!store->available) {
        hal_file_close(&store->file);
    }
    return store->available;
}

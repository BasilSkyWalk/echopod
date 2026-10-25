#include "favorites_store.h"

#include "hal/storage.h"

const char FAVORITES_STORE_PATH[] = "/.echo/favorites.bin";

static bool read_favorites(uint8_t *bytes, uint32_t *length)
{
    HalFile file;
    if (!hal_file_open(&file, FAVORITES_STORE_PATH)) {
        return false;
    }
    *length = file.size < FAVORITES_FILE_CAPACITY ? file.size : FAVORITES_FILE_CAPACITY;
    bool read = hal_file_read(file, 0, bytes, *length);
    hal_file_close(&file);
    return read;
}

void favorites_store_load(FavoritesStore *store, const Library *library)
{
    uint8_t bytes[FAVORITES_FILE_CAPACITY];
    uint32_t length = 0;
    store->library = library;
    store->revision++;
    if (!read_favorites(bytes, &length) || !favorites_decode(bytes, length, &store->favorites)) {
        favorites_clear(&store->favorites);
    }
}

bool favorites_store_contains_album(const FavoritesStore *store, uint16_t album)
{
    return favorites_contains(&store->favorites, library_album_key(store->library, album));
}

static bool save(const FavoritesStore *store)
{
    uint8_t bytes[FAVORITES_FILE_CAPACITY];
    uint32_t length = favorites_encode(&store->favorites, bytes);
    return hal_file_write(FAVORITES_STORE_PATH, bytes, length);
}

FavoriteResult favorites_store_toggle_album(FavoritesStore *store, uint16_t album)
{
    uint32_t key = library_album_key(store->library, album);
    bool present = favorites_contains(&store->favorites, key);
    if (!present && store->favorites.count == FAVORITES_CAPACITY) {
        return (FavoriteResult){.change = FAVORITE_FULL, .saved = true};
    }
    favorites_toggle(&store->favorites, key);
    store->revision++;
    FavoriteChange change = present ? FAVORITE_REMOVED : FAVORITE_ADDED;
    return (FavoriteResult){.change = change, .saved = save(store)};
}

const char *favorites_store_message(FavoriteResult result)
{
    if (!result.saved) {
        return "Could not save favorites";
    }
    switch (result.change) {
    case FAVORITE_ADDED:
        return "Added to Favorites";
    case FAVORITE_REMOVED:
        return "Removed from Favorites";
    case FAVORITE_FULL:
        return "Favorites are full";
    }
    return "";
}

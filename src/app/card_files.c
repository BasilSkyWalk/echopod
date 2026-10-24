#include "card_files.h"

#include <stddef.h>
#include <stdint.h>

#include "cover_cache.h"
#include "favorites_store.h"
#include "hal/storage.h"
#include "library_rescan.h"
#include "library_store.h"
#include "music_check.h"
#include "player.h"
#include "settings_store.h"

enum {
    SMALL_FILE_BYTES = 4096,
    LIBRARY_FILE_BYTES = 4 * 1024 * 1024,
    COVERS_FILE_BYTES = 32 * 1024 * 1024,
    SCRATCH_FILE_BYTES = 24 * 1024 * 1024
};

typedef struct CardFile {
    const char *path;
    uint32_t size;
} CardFile;

static const CardFile CARD_FILES[] = {
    {SETTINGS_STORE_PATH, SMALL_FILE_BYTES},  {PLAYER_RESUME_PATH, SMALL_FILE_BYTES},
    {FAVORITES_STORE_PATH, SMALL_FILE_BYTES}, {LIBRARY_STORE_PATH, LIBRARY_FILE_BYTES},
    {COVER_CACHE_PATH, COVERS_FILE_BYTES},    {MUSIC_CHECK_PATH, SMALL_FILE_BYTES},
    {PLAYER_UP_NEXT_PATH, SMALL_FILE_BYTES},  {LIBRARY_SCRATCH_PATH, SCRATCH_FILE_BYTES},
};

CardFilesReport card_files_prepare(void)
{
    CardFilesReport report = {.library_created = false, .card_full = false};
    for (size_t index = 0; index < sizeof CARD_FILES / sizeof CARD_FILES[0]; index++) {
        HalReserveResult result = hal_file_reserve(CARD_FILES[index].path, CARD_FILES[index].size);
        report.card_full |= result == HAL_RESERVE_NO_SPACE;
        report.library_created |=
            result == HAL_RESERVE_CREATED && CARD_FILES[index].path == LIBRARY_STORE_PATH;
    }
    return report;
}

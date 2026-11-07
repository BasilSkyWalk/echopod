#include "folders_store.h"

#include <stdint.h>

#include "hal/storage.h"

const char FOLDERS_STORE_PATH[] = "/.echo/folders.bin";

void folders_store_load(MusicFolders *folders)
{
    uint8_t bytes[MUSIC_FOLDERS_FILE_BYTES];
    HalFile file;
    folders->count = 0;
    if (!hal_file_open(&file, FOLDERS_STORE_PATH)) {
        return;
    }
    if (file.size >= sizeof bytes && hal_file_read(file, 0, bytes, sizeof bytes)) {
        (void)music_folders_decode(bytes, sizeof bytes, folders);
    }
    hal_file_close(&file);
}

bool folders_store_save(const MusicFolders *folders)
{
    uint8_t bytes[MUSIC_FOLDERS_FILE_BYTES];
    music_folders_encode(folders, bytes);
    return hal_file_write(FOLDERS_STORE_PATH, bytes, sizeof bytes);
}

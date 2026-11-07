#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    MUSIC_FOLDERS_MAX = 32,
    MUSIC_FOLDERS_FILE_BYTES = 12 + 4 * MUSIC_FOLDERS_MAX
};

typedef struct MusicFolders {
    uint32_t count;
    uint32_t keys[MUSIC_FOLDERS_MAX];
} MusicFolders;

void music_folders_encode(const MusicFolders *folders, uint8_t *bytes);
bool music_folders_decode(const uint8_t *bytes, uint32_t size, MusicFolders *folders);
bool music_folders_key(const char *folder, const char *name, uint32_t *key);
bool music_folders_has(const MusicFolders *folders, uint32_t key);
bool music_folders_skips(const MusicFolders *folders, const char *folder, const char *name);

#pragma once

#include <stdbool.h>

#include "library/music_folders.h"

extern const char FOLDERS_STORE_PATH[];

void folders_store_load(MusicFolders *folders);
bool folders_store_save(const MusicFolders *folders);

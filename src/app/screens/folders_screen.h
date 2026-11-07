#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "library/music_folders.h"
#include "ui/list_view.h"
#include "ui/screen.h"
#include "ui/theme.h"
#include "ui/toast.h"

enum {
    FOLDERS_SCREEN_ROWS = MUSIC_FOLDERS_MAX,
    FOLDER_NAMES_CAPACITY = 512,
    FOLDERS_SUMMARY_CAPACITY = 16
};

typedef struct FoldersConfig {
    Toast *toast;
    bool *rescan_requested;
    const Theme *theme;
} FoldersConfig;

typedef struct FolderRow {
    uint32_t key;
    uint16_t label;
    bool off;
    bool was_off;
} FolderRow;

typedef struct FoldersScreen FoldersScreen;

struct FoldersScreen {
    Screen screen;
    FoldersScreen *self;
    FoldersConfig config;
    ListModel model;
    ListView list;
    uint32_t now_ms;
    bool listed;
    int count;
    uint32_t used;
    FolderRow rows[FOLDERS_SCREEN_ROWS];
    char names[FOLDER_NAMES_CAPACITY];
    char summary[FOLDERS_SUMMARY_CAPACITY];
};

Screen *folders_screen_init(FoldersScreen *folders, FoldersConfig config);
ScreenAction folders_screen_open(void *context);
const char *folders_screen_summary(const void *context);

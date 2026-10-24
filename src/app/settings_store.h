#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "player.h"
#include "settings/settings.h"

extern const char SETTINGS_STORE_PATH[];

typedef struct SettingsStore {
    Settings settings;
    uint8_t saved[SETTINGS_FILE_SIZE];
    bool has_saved;
} SettingsStore;

void settings_store_load(SettingsStore *store);
bool settings_store_save(SettingsStore *store);
void settings_store_capture(SettingsStore *store, PlayerStatus playback);
void settings_store_restore_playback(const SettingsStore *store, Player *player, uint32_t seed);
void settings_store_apply_hardware(const Settings *settings);

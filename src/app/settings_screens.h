#pragma once

#include <stdint.h>

#include "core/power_policy.h"
#include "player.h"
#include "screens/bluetooth_screen.h"
#include "screens/equalizer_screen.h"
#include "settings_store.h"
#include "ui/choice_screen.h"
#include "ui/menu_screen.h"
#include "ui/theme.h"
#include "ui/toast.h"

enum {
    SETTINGS_CHOICE_COUNT = 10,
    SETTINGS_BLUETOOTH_ITEM = 2,
    SETTINGS_EQUALIZER_ITEM = 3,
    SETTINGS_RESCAN_ITEM = 11,
    SETTINGS_ITEM_COUNT = SETTINGS_CHOICE_COUNT + 3
};

typedef struct SettingsContext {
    SettingsStore *store;
    Player *player;
    PowerPolicy *power;
    Toast *toast;
    bool *rescan_requested;
    BluetoothScreen *bluetooth;
    EqualizerScreen *equalizer;
    int sleep_option;
} SettingsContext;

typedef struct SettingChoice {
    const char *title;
    const char *const *options;
    int option_count;
    int (*current)(const void *context);
    void (*choose)(void *context, int index);
    const char *(*describe)(int current);
} SettingChoice;

typedef struct SettingsScreens SettingsScreens;

typedef struct SettingsEntry {
    const SettingChoice *choice;
    SettingsScreens *owner;
} SettingsEntry;

struct SettingsScreens {
    SettingsContext context;
    const Theme *theme;
    MenuScreen menu;
    ChoiceScreen choice;
    SettingsEntry entries[SETTINGS_CHOICE_COUNT];
    MenuItem items[SETTINGS_ITEM_COUNT];
};

Screen *settings_screens_init(SettingsScreens *screens, SettingsContext context,
                              const Theme *theme);

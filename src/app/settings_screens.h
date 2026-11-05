#pragma once

#include <stdint.h>

#include "core/i18n.h"
#include "core/power_policy.h"
#include "player.h"
#include "screens/bluetooth_screen.h"
#include "screens/date_time_screen.h"
#include "screens/equalizer_screen.h"
#include "settings_store.h"
#include "ui/choice_screen.h"
#include "ui/menu_screen.h"
#include "ui/theme.h"
#include "ui/toast.h"

enum {
    SETTINGS_CHOICE_COUNT = 25,
    SETTINGS_BLUETOOTH_ITEM = 2,
    SETTINGS_EQUALIZER_ITEM = 3,
    SETTINGS_RESCAN_ITEM = 14,
    SETTINGS_ITEM_COUNT = SETTINGS_CHOICE_COUNT + 4,
    SETTINGS_DATE_TIME_ITEM = SETTINGS_ITEM_COUNT - 1
};

typedef struct SettingsContext {
    SettingsStore *store;
    Player *player;
    PowerPolicy *power;
    Toast *toast;
    bool *rescan_requested;
    BluetoothScreen *bluetooth;
    EqualizerScreen *equalizer;
    DateTimeScreen *date_time;
    int sleep_option;
} SettingsContext;

typedef struct SettingChoice {
    StringId title;
    const StringId *options;
    const char *const *plain_options;
    int option_count;
    int (*current)(const void *context);
    void (*choose)(void *context, int index);
    const char *(*describe)(int current);
    uint8_t slot;
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

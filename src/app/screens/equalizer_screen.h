#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "equalizer_store.h"
#include "ui/list_view.h"
#include "ui/screen.h"
#include "ui/theme.h"
#include "ui/toast.h"

enum {
    EQUALIZER_TEXT_CAPACITY = 24
};

typedef struct EqualizerConfig {
    EqualizerStore *store;
    Toast *toast;
    const Theme *theme;
} EqualizerConfig;

typedef struct EqualizerEditor {
    Screen screen;
    EqualizerConfig config;
    int band;
    bool editing;
    bool bypassed;
    char band_text[EQUALIZER_TEXT_CAPACITY];
    char preamp_text[EQUALIZER_TEXT_CAPACITY];
} EqualizerEditor;

typedef struct EqualizerScreen {
    Screen screen;
    EqualizerConfig config;
    ListModel model;
    ListView list;
    uint32_t now_ms;
    EqualizerEditor editor;
} EqualizerScreen;

Screen *equalizer_screen_init(EqualizerScreen *equalizer, EqualizerConfig config);
ScreenAction equalizer_screen_open(void *context);
const char *equalizer_screen_summary(const void *context);
const char *equalizer_screen_preset_name(EqualizerPreset preset);

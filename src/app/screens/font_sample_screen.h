#pragma once

#include "ui/screen.h"
#include "ui/theme.h"

typedef struct FontSampleScreen {
    Screen screen;
    const Theme *theme;
} FontSampleScreen;

Screen *font_sample_screen_init(FontSampleScreen *sample, const Theme *theme);

#pragma once

#include "ui/screen.h"
#include "ui/theme.h"

typedef struct MessageConfig {
    const char *title;
    const char *message;
    const Theme *theme;
} MessageConfig;

typedef struct MessageScreen {
    Screen screen;
    MessageConfig config;
} MessageScreen;

Screen *message_screen_init(MessageScreen *message, MessageConfig config);

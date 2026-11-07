#pragma once

#include <stdint.h>

#include "ui/screen.h"
#include "ui/theme.h"

enum {
    MESSAGE_SCREEN_LINES = 4
};

typedef struct MessageConfig {
    const char *title;
    const char *message;
    const Theme *theme;
} MessageConfig;

typedef struct MessageLayout {
    const Font *font;
    int width;
    int count;
    uint16_t start[MESSAGE_SCREEN_LINES];
    uint16_t length[MESSAGE_SCREEN_LINES];
} MessageLayout;

typedef struct MessageScreen {
    Screen screen;
    MessageConfig config;
    MessageLayout layout;
} MessageScreen;

Screen *message_screen_init(MessageScreen *message, MessageConfig config);

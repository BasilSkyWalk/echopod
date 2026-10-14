#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/key_tracker.h"
#include "library/library.h"
#include "player.h"
#include "ui/screen.h"
#include "ui/theme.h"

enum {
    NOW_PLAYING_TEXT_CAPACITY = 128
};

typedef enum NowPlayingCommand {
    NOW_PLAYING_NONE,
    NOW_PLAYING_TOGGLE_PAUSE,
    NOW_PLAYING_NEXT,
    NOW_PLAYING_PREVIOUS,
    NOW_PLAYING_VOLUME_UP,
    NOW_PLAYING_VOLUME_DOWN,
    NOW_PLAYING_LEAVE,
    NOW_PLAYING_HOME
} NowPlayingCommand;

typedef struct NowPlayingConfig {
    Player *player;
    const Library *library;
    const Theme *theme;
} NowPlayingConfig;

typedef struct NowPlayingScreen {
    Screen screen;
    NowPlayingConfig config;
    PlayerStatus status;
    bool has_text;
    uint16_t text_track;
    uint32_t text_changed_ms;
    uint32_t now_ms;
    char title[NOW_PLAYING_TEXT_CAPACITY];
    char artist[NOW_PLAYING_TEXT_CAPACITY];
    char album[NOW_PLAYING_TEXT_CAPACITY];
} NowPlayingScreen;

NowPlayingCommand now_playing_command_for(KeyEvent event);
int now_playing_progress_width(uint32_t position_ms, uint32_t duration_ms, int width);
Screen *now_playing_screen_init(NowPlayingScreen *now_playing, NowPlayingConfig config);

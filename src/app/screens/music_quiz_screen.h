#pragma once

#include <stdint.h>

#include "player.h"
#include "ui/list_view.h"
#include "ui/screen.h"
#include "ui/theme.h"

enum {
    MUSIC_QUIZ_CHOICE_COUNT = 4,
    MUSIC_QUIZ_TITLE_CAPACITY = 64
};

typedef struct MusicQuizScreen {
    Screen screen;
    const Theme *theme;
    Player *player;
    ListModel model;
    ListView list;
    uint16_t choices[MUSIC_QUIZ_CHOICE_COUNT];
    uint16_t answer;
    uint32_t random_state;
    uint32_t correct_count;
    uint32_t now_ms;
    bool revealed;
    bool correct;
    bool has_tracks;
    char titles[MUSIC_QUIZ_CHOICE_COUNT][MUSIC_QUIZ_TITLE_CAPACITY];
    char footer[48];
} MusicQuizScreen;

Screen *music_quiz_screen_init(MusicQuizScreen *quiz, Player *player, const Theme *theme);

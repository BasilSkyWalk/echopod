#pragma once

#include <stdint.h>

#include "ui/screen.h"
#include "ui/theme.h"

typedef struct BrickScreen {
    Screen screen;
    const Theme *theme;
    uint16_t bricks[6];
    int ball_x;
    int ball_y;
    int ball_dx;
    int ball_dy;
    int paddle_x;
    uint32_t score;
    uint32_t last_step_ms;
    bool launched;
    bool game_over;
} BrickScreen;

Screen *brick_screen_init(BrickScreen *brick, const Theme *theme);

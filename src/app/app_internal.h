#pragma once

#include "player.h"
#include "ui/theme.h"

PlayerStatus app_player_status(void);
const char *app_toast_text(void);
const char *app_now_playing_lyric(void);
const char *app_diagnostics_value(int index);
void app_forget_clock(void);
void app_now_playing_layout(ThemeLayout *out);

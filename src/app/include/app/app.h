#pragma once

#include "core/frame_stats.h"

void app_start(void);
void app_step(void);
void app_stop(void);
const FrameStats *app_frame_stats(void);
const char *app_screen_title(void);
int app_screen_depth(void);
void app_show_notice(const char *title, const char *message);
void app_wake(void);
void app_show_splash(void);

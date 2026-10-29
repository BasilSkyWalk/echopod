#pragma once

#include <stdbool.h>

#include "core/frame_stats.h"

void app_start(void);
void app_step(void);
void app_stop(void);
const FrameStats *app_frame_stats(void);
const char *app_screen_title(void);
int app_screen_depth(void);
void app_show_notice(const char *title, const char *message);
void app_wake(void);
void app_release_audio(void);
void app_remember_dac_volume(uint8_t level);
void app_request_rescan(void);
void app_request_rescan_if_music_changed(void);
void app_show_splash(void);
bool app_charging(void);

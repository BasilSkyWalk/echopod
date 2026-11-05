#pragma once

#include <stdbool.h>

#include "core/frame_stats.h"
#include "core/strings.h"

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

typedef enum AppDacKey {
    APP_DAC_KEY_NONE,
    APP_DAC_KEY_VOLUME_UP,
    APP_DAC_KEY_VOLUME_DOWN,
    APP_DAC_KEY_PLAY_PAUSE,
    APP_DAC_KEY_NEXT,
    APP_DAC_KEY_PREVIOUS,
    APP_DAC_KEY_LEAVE
} AppDacKey;

void app_dac_keys_start(void);
AppDacKey app_dac_next_key(void);
void app_dac_keys_stop(void);
void app_choice_show(const char *title, const StringId *options, int count, int current);
int app_choice_step(void);
void app_choice_close(void);
void app_request_rescan(void);
void app_request_rescan_if_music_changed(void);
void app_show_splash(void);
bool app_charging(void);
bool app_idle(void);

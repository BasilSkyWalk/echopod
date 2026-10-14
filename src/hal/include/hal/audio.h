#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    HAL_AUDIO_VOLUME_MAX = 50
};

typedef enum HalAudioState {
    HAL_AUDIO_STOPPED,
    HAL_AUDIO_PLAYING,
    HAL_AUDIO_PAUSED
} HalAudioState;

typedef enum HalAudioGain {
    HAL_AUDIO_GAIN_LOW,
    HAL_AUDIO_GAIN_HIGH
} HalAudioGain;

typedef enum HalAudioOutput {
    HAL_AUDIO_OUTPUT_AUTO,
    HAL_AUDIO_OUTPUT_SINGLE_ENDED,
    HAL_AUDIO_OUTPUT_BALANCED
} HalAudioOutput;

typedef struct HalAudioStatus {
    HalAudioState state;
    uint32_t token;
    uint32_t position_ms;
    bool failed;
} HalAudioStatus;

bool hal_audio_play(const char *path, uint32_t start_ms, uint32_t token);
void hal_audio_queue_next(const char *path, uint32_t token);
void hal_audio_pause(void);
void hal_audio_resume(void);
void hal_audio_seek(uint32_t position_ms);
void hal_audio_set_volume(uint8_t level);
void hal_audio_set_gain(HalAudioGain gain);
void hal_audio_set_output(HalAudioOutput output);
HalAudioStatus hal_audio_status(void);

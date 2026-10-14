#include "hal/audio.h"

bool hal_audio_play(const char *path, uint32_t start_ms, uint32_t token)
{
    (void)path;
    (void)start_ms;
    (void)token;
    return false;
}

void hal_audio_queue_next(const char *path, uint32_t token)
{
    (void)path;
    (void)token;
}

void hal_audio_pause(void)
{
}

void hal_audio_resume(void)
{
}

void hal_audio_seek(uint32_t position_ms)
{
    (void)position_ms;
}

void hal_audio_set_volume(uint8_t level)
{
    (void)level;
}

void hal_audio_set_gain(HalAudioGain gain)
{
    (void)gain;
}

void hal_audio_set_output(HalAudioOutput output)
{
    (void)output;
}

HalAudioStatus hal_audio_status(void)
{
    return (HalAudioStatus){
        .state = HAL_AUDIO_STOPPED, .token = 0, .position_ms = 0, .failed = false};
}

#include "sim_speed.h"

#include "dsp/speed.h"

enum {
    MILLISECONDS_PER_SECOND = 1000,
    QUARTERS_PER_UNIT = 4
};

HalAudioSpeed sim_speed_normal(HalAudioSpeed speed)
{
    bool valid = speed.quarters >= HAL_AUDIO_SPEED_MIN_QUARTERS &&
                 speed.quarters <= HAL_AUDIO_SPEED_MAX_QUARTERS &&
                 speed.quarters != HAL_AUDIO_SPEED_UNITY_QUARTERS;
    return valid ? speed : (HalAudioSpeed){.quarters = HAL_AUDIO_SPEED_UNITY_QUARTERS};
}

bool sim_speed_same(HalAudioSpeed a, HalAudioSpeed b)
{
    return sim_speed_normal(a).quarters == sim_speed_normal(b).quarters;
}

uint32_t sim_speed_quarters(HalAudioSpeed speed, uint32_t rate)
{
    return speed_played_quarters((SpeedSetting){speed.quarters}, rate);
}

uint64_t sim_speed_frames(HalAudioSpeed speed, uint32_t milliseconds, uint32_t rate)
{
    return (uint64_t)milliseconds * rate * sim_speed_quarters(speed, rate) /
           (QUARTERS_PER_UNIT * MILLISECONDS_PER_SECOND);
}

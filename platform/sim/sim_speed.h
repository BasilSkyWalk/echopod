#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hal/audio.h"

HalAudioSpeed sim_speed_normal(HalAudioSpeed speed);
bool sim_speed_same(HalAudioSpeed a, HalAudioSpeed b);
uint64_t sim_speed_frames(HalAudioSpeed speed, uint32_t milliseconds, uint32_t rate);

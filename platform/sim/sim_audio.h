#pragma once

#include <stdbool.h>
#include <stdint.h>

bool sim_audio_start(bool virtual_output);
void sim_audio_tick(uint32_t elapsed_ms);
void sim_audio_stop(void);

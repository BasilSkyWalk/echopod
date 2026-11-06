#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hal/audio.h"

HalToneResult dac_path_start(uint32_t sample_rate);
void dac_path_stop(void);
void dac_path_stop_if_up(void);
bool dac_path_is_up(void);
bool dac_path_amp_on(void);
void dac_path_set_fade_service(void (*service)(void));
void dac_path_set_fades(bool fades);
void dac_path_output_on(void);
void dac_path_set_muted(bool mute);
void dac_path_set_volume(uint8_t value);
void dac_path_set_gain(bool high);
void dac_path_set_filter(uint8_t filter);
void dac_path_set_balance(uint8_t balance);
bool dac_path_take_underrun(void);
void dac_path_start_transfer(void);
void dac_path_stop_transfer(void);

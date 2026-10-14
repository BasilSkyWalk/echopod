#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/key.h"

void fake_hal_reset(void);
void fake_hal_add_file(const char *path, const uint8_t *bytes, uint32_t size);
void fake_hal_set_milliseconds(uint32_t now_ms);
void fake_hal_set_microseconds_per_read(uint32_t step_us);
void fake_hal_queue_transition(Key key, bool pressed, uint32_t time_ms);
uint16_t fake_hal_pixel(int x, int y);
const char *fake_hal_audio_playing_path(void);
uint32_t fake_hal_audio_play_start(void);
int fake_hal_audio_play_count(void);
const char *fake_hal_audio_queued_path(void);
void fake_hal_audio_finish_track(void);
void fake_hal_audio_fail_track(void);
void fake_hal_audio_set_position(uint32_t position_ms);
int fake_hal_audio_seek_count(void);
uint32_t fake_hal_audio_last_seek(void);
uint8_t fake_hal_audio_volume(void);
int fake_hal_audio_state(void);
int fake_hal_write_count(void);
long fake_hal_pixels_written(void);
int fake_hal_present_count(void);

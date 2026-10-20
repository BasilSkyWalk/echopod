#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/key.h"
#include "hal/audio.h"
#include "hal/bluetooth.h"
#include "hal/power.h"
#include "hal/storage.h"

void fake_hal_reset(void);
void fake_hal_set_version(const char *version);
void fake_hal_add_file(const char *path, const uint8_t *bytes, uint32_t size);
void fake_hal_set_milliseconds(uint32_t now_ms);
void fake_hal_set_microseconds_per_read(uint32_t step_us);
uint32_t fake_hal_microseconds(void);
void fake_hal_refuse_clock_switch(void);
void fake_hal_audio_set_idle(bool idle);
int fake_hal_test_tone_count(void);
int fake_hal_stall_test_count(void);
int fake_hal_keep_alive_count(void);
void fake_hal_set_restart_pending(bool pending);
void fake_hal_set_reserve_result(HalReserveResult result);
int fake_hal_reserve_count(void);
uint32_t fake_hal_reserved_size(const char *path);
int fake_hal_scratch_borrow_count(void);
void fake_hal_set_empty_card(bool empty);
int fake_hal_dir_open_count(void);
int fake_hal_library_writes(void);
int fake_hal_covers_writes(void);
int fake_hal_scratch_outstanding(void);
int fake_hal_loop_bench_count(void);
int fake_hal_core1_test_count(void);
int fake_hal_core1_decode_count(void);
int fake_hal_core1_accel_count(void);
int fake_hal_audio_service_count(void);
HalBluetoothStatus *fake_hal_bluetooth(void);
bool fake_hal_bluetooth_powered(void);
int fake_hal_bluetooth_scan_count(void);
int fake_hal_bluetooth_service_count(void);
int fake_hal_bluetooth_connect_count(void);
int fake_hal_bluetooth_connect_index(void);
int fake_hal_bluetooth_disconnect_count(void);
void fake_hal_queue_transition(Key key, bool pressed, uint32_t time_ms);
uint16_t fake_hal_pixel(int x, int y);
const char *fake_hal_audio_playing_path(void);
uint32_t fake_hal_audio_play_start(void);
int fake_hal_audio_play_count(void);
const char *fake_hal_audio_queued_path(void);
void fake_hal_audio_finish_track(void);
void fake_hal_audio_fail_track(void);
void fake_hal_audio_refuse_plays(int count);
void fake_hal_audio_set_jack(HalAudioJack jack);
uint32_t fake_hal_dma_tone_rate(void);
void fake_hal_audio_set_refusal(HalAudioRefusal refusal);
void fake_hal_audio_set_position(uint32_t position_ms);
void fake_hal_audio_set_format(HalAudioFormat format);
void fake_hal_audio_set_token(uint32_t token);
int fake_hal_audio_seek_count(void);
uint32_t fake_hal_audio_last_seek(void);
uint8_t fake_hal_audio_volume(void);
int fake_hal_audio_state(void);
int fake_hal_write_count(void);
long fake_hal_pixels_written(void);
int fake_hal_present_count(void);
bool fake_hal_display_on(void);
int fake_hal_presents_at_power_on(void);
uint8_t fake_hal_display_brightness(void);
void fake_hal_set_battery(HalBattery battery);
int fake_hal_power_off_count(void);
int fake_hal_audio_gain(void);
int fake_hal_audio_output(void);
const uint8_t *fake_hal_file_bytes(const char *path, uint32_t *size);

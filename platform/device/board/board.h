#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/battery_gauge.h"

void board_clock_start(void);
void board_clock_record_boot(void);
uint32_t board_boot_ticks_per_microsecond(void);
void board_clock_set_rate(uint32_t ticks_per_microsecond);
uint32_t board_pclk_hz(void);
void board_delay_ms(uint32_t milliseconds);
void board_delay_us(uint32_t microseconds);
void board_display_start(void);
void board_input_start(void);
void board_input_sample(uint32_t now_ms);
BatteryGauge board_battery_gauge(void);
void board_systick_handler(void);
const char *board_audio_current_path(void);
bool board_core1_start_stream(void);
void board_core1_stop(void);
bool board_core1_streaming(void);
void board_audio_before_clock_change(void);
void board_audio_after_clock_change(void);
void board_wait_for_key(void);
void board_main(void);
void board_power_hold(void);
void board_start_record_boot(void);
void board_start_record_tick(uint32_t uptime_ms);
void board_start_record_power_off(void);
void device_fault_handler(const uint32_t *frame);
void device_fault_entry(void);

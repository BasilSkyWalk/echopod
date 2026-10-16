#pragma once

#include <stdint.h>

void board_clock_start(void);
void board_clock_record_boot(void);
uint32_t board_boot_ticks_per_microsecond(void);
void board_clock_set_rate(uint32_t ticks_per_microsecond);
uint32_t board_pclk_hz(void);
void board_delay_ms(uint32_t milliseconds);
void board_delay_us(uint32_t microseconds);
void board_display_start(void);
void board_input_start(void);
void board_input_poll(void);
const char *board_audio_current_path(void);
void board_wait_for_key(void);
void board_main(void);

#pragma once

#include <stdint.h>

void board_clock_start(void);
void board_delay_ms(uint32_t milliseconds);
void board_delay_us(uint32_t microseconds);
void board_display_start(void);
void board_input_start(void);
void board_input_poll(void);
void board_wait_for_key(void);
void board_main(void);

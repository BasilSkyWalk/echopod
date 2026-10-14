#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/key.h"

void fake_hal_reset(void);
void fake_hal_set_milliseconds(uint32_t now_ms);
void fake_hal_set_microseconds_per_read(uint32_t step_us);
void fake_hal_queue_transition(Key key, bool pressed, uint32_t time_ms);
uint16_t fake_hal_pixel(int x, int y);
long fake_hal_pixels_written(void);
int fake_hal_present_count(void);

#pragma once

#include <stdint.h>

void sim_clock_use_virtual_time(uint32_t start_ms);
void sim_clock_advance(uint32_t milliseconds);

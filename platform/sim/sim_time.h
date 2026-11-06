#pragma once

#include <stdbool.h>

#include <stdint.h>

void sim_time_configure(bool absent, bool unset);
void sim_time_start_on(uint16_t year, uint8_t month, uint8_t day);

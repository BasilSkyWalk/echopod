#pragma once

#include <stdbool.h>
#include <stdint.h>

void sim_power_configure(uint8_t battery_percent, bool charging);
bool sim_power_off_requested(void);

#pragma once

#include <stdbool.h>
#include <stdint.h>

bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms);

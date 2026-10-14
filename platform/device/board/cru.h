#pragma once

#include <stdbool.h>
#include <stdint.h>

void cru_gate(uint32_t id, bool clock_on);
void cru_reset(uint32_t id, bool held);
void cru_write(uint32_t offset, uint32_t value);

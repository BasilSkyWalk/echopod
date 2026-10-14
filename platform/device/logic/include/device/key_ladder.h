#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/key.h"

bool key_ladder_decode(uint16_t reading, Key *key);
uint8_t key_ladder_mask(uint16_t reading);

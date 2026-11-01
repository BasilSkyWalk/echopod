#pragma once

#include <stdbool.h>
#include <stdint.h>

bool hal_language_pack(uint32_t language, const uint8_t **pack, uint32_t *size);

#pragma once

#include <stdint.h>

uint32_t image_crc_update(uint32_t crc, const uint8_t *bytes, uint32_t length);

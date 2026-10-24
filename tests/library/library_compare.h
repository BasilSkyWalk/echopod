#pragma once

#include <stdbool.h>
#include <stdint.h>

bool library_compare(const uint8_t *expected, uint32_t expected_size, const uint8_t *actual,
                     uint32_t actual_size);

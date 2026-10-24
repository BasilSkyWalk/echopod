#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool fat_name_matches(const uint16_t *units, size_t limit, const char *name, size_t length);
uint32_t fat_fold(uint32_t code_point);
size_t fat_utf8_from_units(const uint16_t *units, size_t limit, char *out, size_t capacity);

#pragma once

#include <stddef.h>
#include <stdint.h>

void format_milliseconds(uint32_t microseconds, char *buffer, size_t capacity);
void format_duration(uint32_t milliseconds, char *buffer, size_t capacity);

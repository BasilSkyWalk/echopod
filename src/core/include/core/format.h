#pragma once

#include <stddef.h>
#include <stdint.h>

void format_milliseconds(uint32_t microseconds, char *buffer, size_t capacity);
void format_duration(uint32_t milliseconds, char *buffer, size_t capacity);
void format_sample_rate(uint32_t hertz, char *buffer, size_t capacity);
void format_bitrate(uint32_t kbps, char *buffer, size_t capacity);
uint32_t average_kbps(uint32_t audio_bytes, uint32_t total_samples, uint32_t sample_rate);

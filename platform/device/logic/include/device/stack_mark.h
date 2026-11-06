#pragma once

#include <stdint.h>

#define STACK_MARK_PATTERN 0x214B5453u

void stack_mark_paint(uint32_t *bottom, uint32_t words);
uint32_t stack_mark_untouched_words(const uint32_t *bottom, uint32_t words);

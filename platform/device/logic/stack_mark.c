#include "device/stack_mark.h"

void stack_mark_paint(uint32_t *bottom, uint32_t words)
{
    for (uint32_t index = 0; index < words; index++) {
        bottom[index] = STACK_MARK_PATTERN;
    }
}

uint32_t stack_mark_untouched_words(const uint32_t *bottom, uint32_t words)
{
    uint32_t untouched = 0;
    while (untouched < words && bottom[untouched] == STACK_MARK_PATTERN) {
        untouched++;
    }
    return untouched;
}

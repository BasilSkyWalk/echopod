#include <stdint.h>

#include "board.h"
#include "device/stack_mark.h"
#include "hal/system.h"

#define STACK_BOTTOM 0x03000000u
#define STACK_TOP 0x03004000u
#define PROBE_PATTERN 0xA5C3B172u

enum {
    STACK_BYTES = STACK_TOP - STACK_BOTTOM,
    STACK_WORDS = STACK_BYTES / 4,
    PAINT_MARGIN_WORDS = 64
};

static bool painted;

static uint32_t stack_pointer(void)
{
    uint32_t pointer;
    __asm__ volatile("mov %0, sp" : "=r"(pointer));
    return pointer;
}

static bool bottom_is_ram(void)
{
    volatile uint32_t *bottom = (volatile uint32_t *)(uintptr_t)STACK_BOTTOM;
    *bottom = PROBE_PATTERN;
    bool holds = *bottom == PROBE_PATTERN;
    *bottom = ~PROBE_PATTERN;
    return holds && *bottom == ~PROBE_PATTERN;
}

void board_stack_paint(void)
{
    if (!bottom_is_ram()) {
        return;
    }
    uint32_t words = (stack_pointer() - STACK_BOTTOM) / 4;
    words = words > PAINT_MARGIN_WORDS ? words - PAINT_MARGIN_WORDS : 0;
    stack_mark_paint((uint32_t *)(uintptr_t)STACK_BOTTOM, words);
    painted = true;
}

uint32_t board_stack_used(void)
{
    const uint32_t *bottom = (const uint32_t *)(uintptr_t)STACK_BOTTOM;
    return STACK_BYTES - stack_mark_untouched_words(bottom, STACK_WORDS) * 4;
}

HalStackUsage hal_system_stack_usage(void)
{
    if (!painted) {
        return (HalStackUsage){.used = 0, .capacity = 0};
    }
    return (HalStackUsage){.used = board_stack_used(), .capacity = STACK_BYTES};
}

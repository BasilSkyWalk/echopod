#include <stdint.h>

#include "core1_ticks.h"
#include "registers.h"

#define ALL_INTERRUPTS 0xFFFFFFFFu

typedef void (*Handler)(void);

extern uint32_t core1_bss_start;
extern uint32_t core1_bss_end;
extern uint32_t core1_stack_top;

void core1_reset(void);
void core1_default_handler(void);
void core1_main(void);

typedef union Vector {
    const void *stack;
    Handler handler;
} Vector;

__attribute__((section(".core1_vectors"), used)) static const Vector CORE1_VECTORS[16] = {
    {.stack = &core1_stack_top},
    {.handler = core1_reset},
    {.handler = core1_default_handler},
    {.handler = core1_default_handler},
    {.handler = core1_default_handler},
    {.handler = core1_default_handler},
    {.handler = core1_default_handler},
    {.handler = 0},
    {.handler = 0},
    {.handler = 0},
    {.handler = 0},
    {.handler = core1_default_handler},
    {.handler = core1_default_handler},
    {.handler = 0},
    {.handler = core1_default_handler},
    {.handler = core1_systick},
};

volatile uint32_t core1_tick_periods;

void core1_systick(void)
{
    core1_tick_periods++;
}

void core1_default_handler(void)
{
    for (;;) {
        __asm__ volatile("wfi");
    }
}

static void zero_bss(void)
{
    for (uint32_t *word = &core1_bss_start; word < &core1_bss_end; word++) {
        *word = 0;
    }
}

void core1_reset(void)
{
    __asm__ volatile("cpsid i" ::: "memory");
    register_write(SYSTICK_CTRL, 0);
    register_write(NVIC_ICER0, ALL_INTERRUPTS);
    register_write(NVIC_ICER1, ALL_INTERRUPTS);
    register_write(NVIC_ICPR0, ALL_INTERRUPTS);
    register_write(NVIC_ICPR1, ALL_INTERRUPTS);
    register_write(SCB_VTOR, 0);
    zero_bss();
    core1_main();
    core1_default_handler();
}

#include <stdint.h>

#include "board.h"
#include "registers.h"

#define STACK_TOP 0x03004000u
#define CODE_VECTORS_ADDRESS 0x03050000u
#define AIRCR_CLEAR_ACTIVE 0x05FA0002u
#define ALL_INTERRUPTS 0xFFFFFFFFu

typedef void (*Handler)(void);

extern uint32_t device_bss_start;
extern uint32_t device_bss_end;

void device_reset(void);
void device_default_handler(void);

__attribute__((section(".entry_vector"), used)) static const Handler ENTRY_VECTOR[2] = {
    (Handler)STACK_TOP,
    device_reset,
};

__attribute__((section(".code_vectors"), used,
               aligned(256))) static const Handler CODE_VECTORS[16] = {
    (Handler)STACK_TOP,
    device_reset,
    device_default_handler,
    device_default_handler,
    device_default_handler,
    device_default_handler,
    device_default_handler,
    0,
    0,
    0,
    0,
    device_default_handler,
    device_default_handler,
    0,
    device_default_handler,
    device_default_handler,
};

void device_default_handler(void)
{
    for (;;) {
    }
}

static void quiet_interrupts(void)
{
    __asm__ volatile("cpsid i" ::: "memory");
    register_write(SYSTICK_CTRL, 0);
    register_write(NVIC_ICER0, ALL_INTERRUPTS);
    register_write(NVIC_ICER1, ALL_INTERRUPTS);
    register_write(NVIC_ICPR0, ALL_INTERRUPTS);
    register_write(NVIC_ICPR1, ALL_INTERRUPTS);
    register_write(SCB_SHCSR, 0);
    register_write(SCB_AIRCR, AIRCR_CLEAR_ACTIVE);
}

static void zero_bss(void)
{
    for (volatile uint32_t *word = &device_bss_start; word < &device_bss_end; word++) {
        *word = 0;
    }
}

static void use_code_vectors(void)
{
    register_write(SCB_VTOR, CODE_VECTORS_ADDRESS);
    __asm__ volatile("dsb\n\tisb" ::: "memory");
}

void device_reset(void)
{
    quiet_interrupts();
    zero_bss();
    use_code_vectors();
    board_main();
    for (;;) {
    }
}

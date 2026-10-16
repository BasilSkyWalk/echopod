#pragma once

#include <stdint.h>

static inline uint32_t irq_save(void)
{
    uint32_t primask;
    __asm__ volatile("mrs %0, primask\n\tcpsid i" : "=r"(primask)::"memory");
    return primask;
}

static inline void irq_restore(uint32_t primask)
{
    __asm__ volatile("msr primask, %0" ::"r"(primask) : "memory");
}

static inline void irq_enable(void)
{
    __asm__ volatile("cpsie i" ::: "memory");
}

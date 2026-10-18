#pragma once

#include <stdint.h>

#define GPIO0_BASE 0x40160000u
#define GPIO1_BASE 0x40170000u
#define GPIO2_BASE 0x50030000u
#define GPIO_DATA 0x00u
#define GPIO_DIRECTION 0x04u
#define GPIO_INPUT_LEVEL 0x50u
#define GRF_BASE 0x50010000u
#define SARADC_BASE 0x400D0000u
#define PWM_BASE 0x400F0000u
#define CRU_BASE 0x40180000u
#define SD_BASE 0x60020000u
#define EMMC_BASE 0x60030000u
#define SYSTICK_CTRL 0xE000E010u
#define SYSTICK_RELOAD 0xE000E014u
#define SYSTICK_CURRENT 0xE000E018u
#define NVIC_ICER0 0xE000E180u
#define NVIC_ICER1 0xE000E184u
#define NVIC_ICPR0 0xE000E280u
#define NVIC_ICPR1 0xE000E284u
#define SCB_ICSR 0xE000ED04u
#define SCB_ICSR_PENDSTSET (1u << 26)
#define SCB_ICSR_PENDSTCLR (1u << 25)
#define SCB_VTOR 0xE000ED08u
#define SCB_AIRCR 0xE000ED0Cu
#define SCB_SHCSR 0xE000ED24u

static inline volatile uint32_t *register_at(uint32_t address)
{
    return (volatile uint32_t *)(uintptr_t)address;
}

static inline uint32_t register_read(uint32_t address)
{
    return *register_at(address);
}

static inline void register_write(uint32_t address, uint32_t value)
{
    *register_at(address) = value;
}

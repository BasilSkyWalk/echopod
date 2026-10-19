#include <stdint.h>

#include "board.h"
#include "hal/system.h"

#define HIGH_RAM_BASE 0x0307A000u
#define HIGH_RAM_END 0x0309F000u
#define PATTERN_SEED 0xA5C3F00Fu

enum {
    PROBE_BYTES = 8,
    PROBE_INPUT = 5,
    PROBE_EXPECTED = 22
};

__attribute__((naked, noinline)) static uint32_t probe(__attribute__((unused)) uint32_t value)
{
    __asm__ volatile("lsls r1, r0, #1\n\tadds r0, r0, r1\n\tadds r0, #7\n\tbx lr");
}

static uint32_t pattern(uint32_t address, uint32_t flip)
{
    return (address ^ PATTERN_SEED ^ address << 7) ^ flip;
}

static uint32_t check_memory(uint32_t flip)
{
    volatile uint32_t *words = (volatile uint32_t *)HIGH_RAM_BASE;
    uint32_t count = (HIGH_RAM_END - HIGH_RAM_BASE) / 4;
    for (uint32_t index = 0; index < count; index++) {
        words[index] = pattern(HIGH_RAM_BASE + index * 4, flip);
    }
    for (uint32_t index = 0; index < count; index++) {
        if (words[index] != pattern(HIGH_RAM_BASE + index * 4, flip)) {
            return HIGH_RAM_BASE + index * 4;
        }
    }
    return 0;
}

static uint32_t call_from_high_ram(void)
{
    const volatile uint8_t *source = (const volatile uint8_t *)((uintptr_t)probe & ~(uintptr_t)1);
    volatile uint8_t *target = (volatile uint8_t *)HIGH_RAM_BASE;
    for (uint32_t index = 0; index < PROBE_BYTES; index++) {
        target[index] = source[index];
    }
    __asm__ volatile("dsb\n\tisb" ::: "memory");
    uint32_t (*copied)(uint32_t) = (uint32_t (*)(uint32_t))(uintptr_t)(HIGH_RAM_BASE | 1u);
    return copied(PROBE_INPUT);
}

HalHighRamTest hal_system_high_ram_test(void)
{
    board_audio_shut_down();
    uint32_t bad = check_memory(0);
    bad = bad != 0 ? bad : check_memory(0xFFFFFFFFu);
    if (bad != 0) {
        return (HalHighRamTest){.status = HAL_HIGH_RAM_BAD_MEMORY, .bad_address = bad};
    }
    uint32_t result = call_from_high_ram();
    HalHighRamStatus status = result == PROBE_EXPECTED ? HAL_HIGH_RAM_OK : HAL_HIGH_RAM_BAD_CALL;
    return (HalHighRamTest){
        .status = status, .size = HIGH_RAM_END - HIGH_RAM_BASE, .call_result = result};
}

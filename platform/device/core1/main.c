#include <stddef.h>
#include <stdint.h>

#include "codec/flac.h"
#include "core1_region.h"
#include "device/core1_control.h"
#include "device/tick_clock.h"
#include "hal/system.h"
#include "loop_bench.h"
#include "registers.h"

#define SYSTICK_ENABLE_CORE_CLOCK 0x5u

void core1_main(void);

static FlacDecoder decoder;
static Core1Stream stream;
static uint32_t input_bytes;

static void start_systick(void)
{
    register_write(SYSTICK_RELOAD, TICK_CLOCK_COUNTER_MASK);
    register_write(SYSTICK_CURRENT, 0);
    register_write(SYSTICK_CTRL, SYSTICK_ENABLE_CORE_CLOCK);
}

static uint32_t ticks_since(uint32_t start)
{
    return (start - register_read(SYSTICK_CURRENT)) & TICK_CLOCK_COUNTER_MASK;
}

uint32_t loop_bench_now(void)
{
    return register_read(SYSTICK_CURRENT);
}

uint32_t loop_bench_since(uint32_t start)
{
    return ticks_since(start);
}

static bool read_input(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    (void)context;
    if (offset > input_bytes || length > input_bytes - offset) {
        return false;
    }
    const volatile uint8_t *source =
        (const volatile uint8_t *)(uintptr_t)(CORE1_INPUT_BASE + offset);
    uint8_t *out = buffer;
    for (uint32_t index = 0; index < length; index++) {
        out[index] = source[index];
    }
    return true;
}

static FlacInfo shared_info(void)
{
    return (FlacInfo){
        .sample_rate = register_read(CORE1_SHARED_SAMPLE_RATE),
        .channels = (uint8_t)register_read(CORE1_SHARED_CHANNELS),
        .bits_per_sample = (uint8_t)register_read(CORE1_SHARED_BITS),
        .min_block = (uint16_t)register_read(CORE1_SHARED_MIN_BLOCK),
        .max_block = (uint16_t)register_read(CORE1_SHARED_MAX_BLOCK),
        .total_samples = register_read(CORE1_SHARED_TOTAL_SAMPLES),
    };
}

static void run_decode_test(void)
{
    input_bytes = register_read(CORE1_SHARED_INPUT_BYTES);
    if (input_bytes > CORE1_INPUT_BYTES) {
        input_bytes = CORE1_INPUT_BYTES;
    }
    flac_start(&decoder, (FlacInput){.read = read_input, .context = NULL, .size = input_bytes},
               shared_info());
    uint32_t blocks = 0;
    uint32_t total = 0;
    uint32_t longest = 0;
    FlacResult result = FLAC_OK;
    for (;;) {
        uint32_t start = register_read(SYSTICK_CURRENT);
        result = flac_decode_frame(&decoder);
        uint32_t ticks = ticks_since(start);
        if (result != FLAC_OK) {
            break;
        }
        blocks++;
        total += ticks;
        longest = ticks > longest ? ticks : longest;
    }
    register_write(CORE1_SHARED_BLOCKS, blocks);
    register_write(CORE1_SHARED_TICKS_TOTAL, total);
    register_write(CORE1_SHARED_TICKS_LONGEST, longest);
    register_write(CORE1_SHARED_LAST_RESULT, (uint32_t)result);
}

void core1_main(void)
{
    start_systick();
    HalLoopCycles cycles = hal_system_loop_cycles();
    register_write(CORE1_SHARED_REGISTER_CENTI, cycles.register_centi);
    register_write(CORE1_SHARED_MEMORY_CENTI, cycles.memory_centi);
    if (register_read(CORE1_SHARED_COMMAND) == CORE1_COMMAND_DECODE) {
        run_decode_test();
    }
    register_write(CORE1_SHARED_READY, CORE1_READY_MAGIC);
    __asm__ volatile("dsb" ::: "memory");
    register_write(CORE1_MAILBOX_B2A_COMMAND, CORE1_READY_COMMAND);
    register_write(CORE1_MAILBOX_B2A_DATA, 0);
    if (register_read(CORE1_SHARED_COMMAND) != CORE1_COMMAND_STREAM) {
        return;
    }
    core1_stream_init(&stream, core1_region(), &decoder);
    for (;;) {
        core1_stream_step(&stream);
    }
}

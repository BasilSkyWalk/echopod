#include <stddef.h>
#include <stdint.h>

#include "codec/alac.h"
#include "codec/flac.h"
#include "core1_region.h"
#include "core1_ticks.h"
#include "device/core1_alac.h"
#include "device/core1_control.h"
#include "device/flac_accel.h"
#include "device/tick_clock.h"
#include "hal/system.h"
#include "loop_bench.h"
#include "registers.h"

#define SYSTICK_ENABLE_CORE_CLOCK 0x5u

void core1_main(void);

static union {
    FlacDecoder flac;
    AlacDecoder alac;
} decoders;
static Core1Flac flac;
static Core1Alac alac;
static Core1Decoder flac_stream;
static FlacLpcHook lpc_hook;
static AccelHook accel_hook;
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

static uint32_t accel_read(void *context, uint32_t address)
{
    (void)context;
    return register_read(address);
}

static void accel_write(void *context, uint32_t address, uint32_t value)
{
    (void)context;
    register_write(address, value);
}

static const AccelBus ACCEL_BUS = {accel_read, accel_write, NULL};

static void run_accel_test(void)
{
    int32_t *scratch = (int32_t *)(uintptr_t)CORE1_STREAM_SAMPLES;
    AccelTest test = flac_accel_self_test(&ACCEL_BUS, scratch, CORE1_STREAM_SAMPLES);
    uint32_t status = (test.narrow_ok ? CORE1_ACCEL_NARROW : 0u) |
                      (test.wide_ok ? CORE1_ACCEL_WIDE : 0u) |
                      (test.timed_out ? CORE1_ACCEL_TIMED_OUT : 0u);
    register_write(CORE1_SHARED_ACCEL_STATUS, status);
    register_write(CORE1_SHARED_ACCEL_FAILED,
                   (uint32_t)test.failed_vector << 16 | (test.failed_sample & 0xFFFFu));
    if (test.timed_out) {
        return;
    }
    AccelBlock block = flac_accel_timing_block(scratch);
    uint32_t start = register_read(SYSTICK_CURRENT);
    bool done = flac_accel_restore(&ACCEL_BUS, CORE1_STREAM_SAMPLES, block.samples,
                                   block.coefficients, block.order, block.shift);
    register_write(CORE1_SHARED_ACCEL_HARDWARE_TICKS, done ? ticks_since(start) : 0u);
    block = flac_accel_timing_block(scratch);
    start = register_read(SYSTICK_CURRENT);
    flac_lpc_restore(scratch, block.samples, block.coefficients, block.order, block.shift);
    register_write(CORE1_SHARED_ACCEL_SOFTWARE_TICKS, ticks_since(start));
}

static void use_accelerator(uint32_t classes)
{
    if ((classes & (CORE1_ACCEL_NARROW | CORE1_ACCEL_WIDE)) == 0) {
        return;
    }
    flac_accel_start(&ACCEL_BUS);
    accel_hook = (AccelHook){.bus = &ACCEL_BUS,
                             .narrow = (classes & CORE1_ACCEL_NARROW) != 0,
                             .wide = (classes & CORE1_ACCEL_WIDE) != 0};
    lpc_hook = flac_accel_hook(&accel_hook);
    flac_set_lpc_hook(&decoders.flac, lpc_hook);
}

static void start_flac(void *context, FlacInput input, FlacInfo info, uint32_t first_sample,
                       const bool *starved)
{
    (void)context;
    flac_set_lpc_hook(&decoders.flac, lpc_hook);
    flac_stream.start(flac_stream.context, input, info, first_sample, starved);
}

static Core1DecodeResult decode_flac(void *context, Core1Block *block)
{
    (void)context;
    return flac_stream.decode(flac_stream.context, block);
}

static void run_decode_test(void)
{
    use_accelerator(register_read(CORE1_SHARED_ACCEL_CLASSES));
    input_bytes = register_read(CORE1_SHARED_INPUT_BYTES);
    if (input_bytes > CORE1_INPUT_BYTES) {
        input_bytes = CORE1_INPUT_BYTES;
    }
    flac_start(&decoders.flac,
               (FlacInput){.read = read_input, .context = NULL, .size = input_bytes},
               shared_info());
    uint32_t blocks = 0;
    uint32_t total = 0;
    uint32_t longest = 0;
    FlacResult result = FLAC_OK;
    for (;;) {
        uint32_t start = register_read(SYSTICK_CURRENT);
        result = flac_decode_frame(&decoders.flac);
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
    register_write(CORE1_SHARED_ACCEL_RESTORED, accel_hook.restored);
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
    if (register_read(CORE1_SHARED_COMMAND) == CORE1_COMMAND_ACCEL) {
        run_accel_test();
    }
    register_write(CORE1_SHARED_READY, CORE1_READY_MAGIC);
    __asm__ volatile("dsb" ::: "memory");
    register_write(CORE1_MAILBOX_B2A_COMMAND, CORE1_READY_COMMAND);
    register_write(CORE1_MAILBOX_B2A_DATA, 0);
    if (register_read(CORE1_SHARED_COMMAND) != CORE1_COMMAND_STREAM) {
        return;
    }
    use_accelerator(register_read(CORE1_SHARED_ACCEL_CLASSES));
    flac_stream = core1_flac_decoder(&flac, &decoders.flac);
    core1_stream_init(&stream, core1_region(), (Core1Decoder){start_flac, decode_flac, NULL});
    core1_stream_set_decoder(&stream, STREAM_CODEC_ALAC, core1_alac_decoder(&alac, &decoders.alac));
    core1_ticks_measure(&stream);
    for (;;) {
        core1_stream_step(&stream);
    }
}

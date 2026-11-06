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

_Static_assert(sizeof(AlacDecoder) <= sizeof(int32_t[FLAC_MAX_BLOCK]),
               "the ALAC decoder must fit in the FLAC program's second bank");

void core1_main(void);

static FlacDecoder flac_decoder;
static Core1Flac flac;
static Core1Alac alac;
static Core1Decoder flac_stream;
static FlacLpcHook lpc_hook;
static AccelHook accel_hook;
static Core1Stream stream;
static Equalizer equalizer;

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

static uint32_t classes_of(AccelTest test, uint32_t narrow, uint32_t wide)
{
    return (test.narrow_ok && !test.timed_out ? narrow : 0u) |
           (test.wide_ok && !test.timed_out ? wide : 0u);
}

static void time_accelerator(int32_t *scratch, uint32_t address)
{
    AccelBlock block = flac_accel_timing_block(scratch);
    uint32_t start = register_read(SYSTICK_CURRENT);
    bool done = flac_accel_restore(&ACCEL_BUS, address, block.samples, block.coefficients,
                                   block.order, block.shift);
    register_write(CORE1_SHARED_ACCEL_HARDWARE_TICKS, done ? ticks_since(start) : 0u);
    block = flac_accel_timing_block(scratch);
    start = register_read(SYSTICK_CURRENT);
    flac_lpc_restore(scratch, block.samples, block.coefficients, block.order, block.shift);
    register_write(CORE1_SHARED_ACCEL_SOFTWARE_TICKS, ticks_since(start));
}

static void run_accel_test(void)
{
    int32_t *data = (int32_t *)(uintptr_t)CORE1_FLAC_BANK_1;
    AccelTest test = flac_accel_self_test(&ACCEL_BUS, data, CORE1_FLAC_BANK_1);
    uint32_t status = classes_of(test, CORE1_ACCEL_NARROW, CORE1_ACCEL_WIDE) |
                      (test.timed_out ? CORE1_ACCEL_TIMED_OUT : 0u);
    register_write(CORE1_SHARED_ACCEL_FAILED,
                   (uint32_t)test.failed_vector << 16 | (test.failed_sample & 0xFFFFu));
    if (!test.timed_out) {
        AccelTest code = flac_accel_self_test(&ACCEL_BUS, (int32_t *)(uintptr_t)CORE1_FLAC_BANK_0,
                                              CORE1_FLAC_BANK_0);
        status |= classes_of(code, CORE1_ACCEL_CODE_NARROW, CORE1_ACCEL_CODE_WIDE);
        time_accelerator(data, CORE1_FLAC_BANK_1);
    }
    register_write(CORE1_SHARED_ACCEL_STATUS, status);
}

static void use_accelerator(uint32_t classes)
{
    if ((classes & (CORE1_ACCEL_NARROW | CORE1_ACCEL_WIDE)) == 0) {
        return;
    }
    flac_accel_start(&ACCEL_BUS);
    accel_hook = (AccelHook){.bus = &ACCEL_BUS,
                             .narrow = (classes & CORE1_ACCEL_NARROW) != 0,
                             .wide = (classes & CORE1_ACCEL_WIDE) != 0,
                             .code_narrow = (classes & CORE1_ACCEL_CODE_NARROW) != 0,
                             .code_wide = (classes & CORE1_ACCEL_CODE_WIDE) != 0};
    lpc_hook = flac_accel_hook(&accel_hook);
    flac_set_lpc_hook(&flac_decoder, lpc_hook);
}

static void start_flac(void *context, FlacInput input, FlacInfo info, uint32_t first_sample,
                       const bool *starved)
{
    (void)context;
    flac_set_lpc_hook(&flac_decoder, lpc_hook);
    flac_stream.start(flac_stream.context, input, info, first_sample, starved);
}

static Core1DecodeResult decode_flac(void *context, Core1Block *block)
{
    (void)context;
    return flac_stream.decode(flac_stream.context, block);
}

void core1_main(void)
{
    start_systick();
    HalLoopCycles cycles = hal_system_loop_cycles();
    register_write(CORE1_SHARED_REGISTER_CENTI, cycles.register_centi);
    register_write(CORE1_SHARED_MEMORY_CENTI, cycles.memory_centi);
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
    Core1FlacBanks banks = {(int32_t *)(uintptr_t)CORE1_FLAC_BANK_0,
                            (int32_t *)(uintptr_t)CORE1_FLAC_BANK_1};
    flac_stream = core1_flac_decoder(&flac, &flac_decoder, banks);
    core1_stream_init(&stream, core1_region(), (Core1Decoder){start_flac, decode_flac, NULL});
    core1_stream_set_slots(&stream, CORE1_FLAC_SLOT_OFFSET, CORE1_FLAC_SLOT_SAMPLES);
    core1_stream_set_decoder(
        &stream, STREAM_CODEC_ALAC,
        core1_alac_decoder(&alac, (AlacDecoder *)(uintptr_t)CORE1_FLAC_BANK_1));
    core1_stream_set_equalizer(&stream, &equalizer,
                               (volatile Core1EqualizerShare *)(uintptr_t)CORE1_SHARED_EQUALIZER);
    core1_ticks_start();
    core1_ticks_measure(&stream);
    for (;;) {
        core1_stream_serve(&stream);
    }
}

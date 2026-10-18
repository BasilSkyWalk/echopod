#include <stddef.h>
#include <stdint.h>

#include "board.h"
#include "codec/flac.h"
#include "device/core1_control.h"
#include "hal/clock.h"
#include "hal/storage.h"
#include "hal/system.h"
#include "registers.h"

enum {
    LOWEST_PLL_MHZ = 96,
    CORE1_RAM_BYTES = 0x60000,
    STAGING_WORDS = 1024,
    WORD_BYTES = 4,
    DECODE_TIMEOUT_MS = 10000,
    REPLY_TIMEOUT_MS = 200,
    MICROSECONDS_PER_SECOND = 1000000
};

static uint32_t staging[STAGING_WORDS];
static bool streaming;
static FlacBits header_bits;

extern const uint8_t _binary_core1_bin_start[];
extern const uint8_t _binary_core1_bin_end[];

static uint32_t bus_read(void *context, uint32_t address)
{
    (void)context;
    return register_read(address);
}

static void bus_write(void *context, uint32_t address, uint32_t value)
{
    (void)context;
    register_write(address, value);
}

static void bus_delay_ms(void *context, uint32_t milliseconds)
{
    (void)context;
    board_delay_ms(milliseconds);
}

static uint32_t bus_now_ms(void *context)
{
    (void)context;
    return hal_clock_milliseconds();
}

static const Core1Bus BUS = {bus_read, bus_write, bus_delay_ms, bus_now_ms, NULL};

static HalCore1Status status_of(Core1Result result)
{
    switch (result) {
    case CORE1_OK:
        return HAL_CORE1_RAN;
    case CORE1_NO_POWER:
        return HAL_CORE1_NO_POWER;
    case CORE1_NO_REPLY:
        return HAL_CORE1_NO_REPLY;
    case CORE1_BAD_REPLY:
        return HAL_CORE1_BAD_REPLY;
    case CORE1_NO_STOP:
        return HAL_CORE1_NO_STOP;
    case CORE1_TOO_BIG:
        return HAL_CORE1_BAD_REPLY;
    }
    return HAL_CORE1_BAD_REPLY;
}

bool board_core1_start_stream(void)
{
    if (streaming) {
        return true;
    }
    uint32_t pll_mhz = hal_system_clock_mhz();
    if (pll_mhz < LOWEST_PLL_MHZ) {
        return false;
    }
    board_start_record_step(HAL_START_STEP_CORE1_START);
    uint32_t divider = core1_divider_for(pll_mhz);
    uint32_t length = (uint32_t)(_binary_core1_bin_end - _binary_core1_bin_start);
    Core1Result result = core1_power_up(&BUS, pll_mhz, divider);
    if (result == CORE1_OK) {
        result = core1_load(&BUS, _binary_core1_bin_start, length);
    }
    if (result == CORE1_OK) {
        register_write(CORE1_SHARED_COMMAND, CORE1_COMMAND_STREAM);
        result = core1_run(&BUS, REPLY_TIMEOUT_MS);
    }
    board_start_record_step_done();
    if (result != CORE1_OK) {
        core1_stop(&BUS);
        return false;
    }
    streaming = true;
    return true;
}

void board_core1_stop(void)
{
    if (streaming) {
        board_start_record_step(HAL_START_STEP_CORE1_STOP);
        core1_stop(&BUS);
        streaming = false;
        board_start_record_step_done();
    }
}

bool board_core1_streaming(void)
{
    return streaming;
}

HalCore1Test hal_system_core1_test(void)
{
    if (streaming) {
        return (HalCore1Test){.status = HAL_CORE1_BUSY};
    }
    uint32_t pll_mhz = hal_system_clock_mhz();
    if (pll_mhz < LOWEST_PLL_MHZ) {
        return (HalCore1Test){.status = HAL_CORE1_NEEDS_PLL};
    }
    uint32_t divider = core1_divider_for(pll_mhz);
    HalCore1Test test = {.status = HAL_CORE1_RAN, .mhz = pll_mhz / divider};
    Core1Result result = core1_power_up(&BUS, pll_mhz, divider);
    if (result == CORE1_OK) {
        uint32_t length = (uint32_t)(_binary_core1_bin_end - _binary_core1_bin_start);
        result = core1_start(&BUS, _binary_core1_bin_start, length);
    }
    if (result == CORE1_OK) {
        test.cycles = (HalLoopCycles){.register_centi = register_read(CORE1_SHARED_REGISTER_CENTI),
                                      .memory_centi = register_read(CORE1_SHARED_MEMORY_CENTI)};
    }
    Core1Result stopped = core1_stop(&BUS);
    test.status = status_of(result != CORE1_OK ? result : stopped);
    return test;
}

static bool read_file(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    const HalFile *file = context;
    return hal_file_read(*file, offset, buffer, length);
}

static bool copy_audio(const HalFile *file, uint32_t audio_offset, uint32_t length)
{
    for (uint32_t done = 0; done < length; done += sizeof staging) {
        uint32_t chunk = length - done < sizeof staging ? length - done : (uint32_t)sizeof staging;
        if (!hal_file_read(*file, audio_offset + done, staging, chunk)) {
            return false;
        }
        for (uint32_t word = 0; word * WORD_BYTES < chunk; word++) {
            register_write(CORE1_INPUT_BASE + done + word * WORD_BYTES, staging[word]);
        }
    }
    return true;
}

static void share_info(FlacInfo info, uint32_t input_bytes)
{
    register_write(CORE1_SHARED_COMMAND, CORE1_COMMAND_DECODE);
    register_write(CORE1_SHARED_INPUT_BYTES, input_bytes);
    register_write(CORE1_SHARED_SAMPLE_RATE, info.sample_rate);
    register_write(CORE1_SHARED_CHANNELS, info.channels);
    register_write(CORE1_SHARED_BITS, info.bits_per_sample);
    register_write(CORE1_SHARED_MIN_BLOCK, info.min_block);
    register_write(CORE1_SHARED_MAX_BLOCK, info.max_block);
    register_write(CORE1_SHARED_TOTAL_SAMPLES, info.total_samples);
}

static HalCore1Decode decode_results(HalCore1Decode result, FlacInfo info)
{
    uint32_t blocks = register_read(CORE1_SHARED_BLOCKS);
    uint32_t ticks_per_us = result.mhz == 0 ? 1 : result.mhz;
    result.blocks = blocks;
    result.average_us =
        blocks == 0 ? 0 : register_read(CORE1_SHARED_TICKS_TOTAL) / blocks / ticks_per_us;
    result.longest_us = register_read(CORE1_SHARED_TICKS_LONGEST) / ticks_per_us;
    result.block_us =
        info.sample_rate == 0
            ? 0
            : (uint32_t)((uint64_t)info.max_block * MICROSECONDS_PER_SECOND / info.sample_rate);
    return result;
}

static Core1Result run_decode(const HalFile *file, FlacInfo info, uint32_t audio_offset,
                              uint32_t pll_mhz, uint32_t divider, bool *read_failed)
{
    Core1Result result = core1_power_up(&BUS, pll_mhz, divider);
    if (result != CORE1_OK) {
        return result;
    }
    uint32_t length = (uint32_t)(_binary_core1_bin_end - _binary_core1_bin_start);
    result = core1_load(&BUS, _binary_core1_bin_start, length);
    if (result != CORE1_OK) {
        return result;
    }
    uint32_t available = file->size - audio_offset;
    uint32_t input_bytes = available < CORE1_INPUT_BYTES ? available : CORE1_INPUT_BYTES;
    if (!copy_audio(file, audio_offset, input_bytes)) {
        *read_failed = true;
        return CORE1_OK;
    }
    share_info(info, input_bytes);
    return core1_run(&BUS, DECODE_TIMEOUT_MS);
}

HalCore1Decode hal_system_core1_decode_test(void)
{
    if (streaming) {
        return (HalCore1Decode){.status = HAL_CORE1_BUSY};
    }
    uint32_t pll_mhz = hal_system_clock_mhz();
    if (pll_mhz < LOWEST_PLL_MHZ) {
        return (HalCore1Decode){.status = HAL_CORE1_NEEDS_PLL};
    }
    const char *path = board_audio_current_path();
    if (path == NULL) {
        return (HalCore1Decode){.status = HAL_CORE1_NO_TRACK};
    }
    HalFile file;
    if (!hal_file_open(&file, path)) {
        return (HalCore1Decode){.status = HAL_CORE1_READ_FAILED};
    }
    FlacInfo info;
    uint32_t audio_offset = 0;
    FlacInput input = {.read = read_file, .context = &file, .size = file.size};
    if (flac_read_header(&header_bits, input, &info, &audio_offset) != FLAC_OK) {
        hal_file_close(&file);
        return (HalCore1Decode){.status = HAL_CORE1_NOT_FLAC};
    }
    uint32_t divider = core1_divider_for(pll_mhz);
    HalCore1Decode result = {.status = HAL_CORE1_RAN, .mhz = pll_mhz / divider};
    bool read_failed = false;
    Core1Result ran = run_decode(&file, info, audio_offset, pll_mhz, divider, &read_failed);
    if (ran == CORE1_OK && !read_failed) {
        result = decode_results(result, info);
    }
    Core1Result stopped = core1_stop(&BUS);
    hal_file_close(&file);
    if (read_failed) {
        result.status = HAL_CORE1_READ_FAILED;
    } else {
        result.status = status_of(ran != CORE1_OK ? ran : stopped);
    }
    return result;
}

static bool lent;

void *hal_system_borrow_scratch(uint32_t *size)
{
    board_audio_shut_down();
    board_core1_stop();
    if (hal_system_clock_mhz() < LOWEST_PLL_MHZ) {
        hal_system_set_clock_mhz(LOWEST_PLL_MHZ);
    }
    uint32_t pll_mhz = hal_system_clock_mhz();
    *size = 0;
    if (core1_power_up(&BUS, pll_mhz, core1_divider_for(pll_mhz)) != CORE1_OK) {
        core1_stop(&BUS);
        return NULL;
    }
    lent = true;
    *size = CORE1_RAM_BYTES;
    return (void *)(uintptr_t)CORE1_CODE_BASE;
}

void hal_system_return_scratch(void)
{
    if (lent) {
        core1_stop(&BUS);
        lent = false;
    }
}

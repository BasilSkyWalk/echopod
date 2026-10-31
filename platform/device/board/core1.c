#include <stddef.h>
#include <stdint.h>

#include "board.h"
#include "device/core1_control.h"
#include "device/core1_stream.h"
#include "device/image_crc.h"
#include "emmc.h"
#include "hal/clock.h"
#include "hal/system.h"
#include "registers.h"

enum {
    LOWEST_PLL_MHZ = 96,
    FULL_SPEED_MHZ = 200,
    CORE1_RAM_BYTES = 0x60000,
    STAGING_WORDS = 1024,
    WORD_BYTES = 4,
    DECODE_TIMEOUT_MS = 10000,
    REPLY_TIMEOUT_MS = 200,
    SECTOR_BYTES = 512,
    MP3_PROGRAM_OFFSET = 0x240000,
    FLAC_PROGRAM_OFFSET = 0x280000,
    AAC_PROGRAM_OFFSET = 0x2A0200,
    OPUS_PROGRAM_OFFSET = 0x218000,
    PROGRAM_MAGIC = 0x4D314345,
    STAGING_SECTORS = STAGING_WORDS * WORD_BYTES / SECTOR_BYTES
};

static uint32_t staging[STAGING_WORDS];
static bool streaming;
static bool accel_known;
static uint32_t accel_classes;

static HalCore1Accel run_accel_test(void);
static Core1Program streaming_program;
static uint32_t streaming_divider;
static EqualizerCurve equalizer_curve;
static uint32_t equalizer_version;

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

static uint32_t le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static bool copy_program_sectors(uint32_t first_sector, uint32_t size, uint32_t expected_crc)
{
    uint32_t crc = 0;
    for (uint32_t done = 0; done < size; done += sizeof staging) {
        uint32_t chunk = size - done < sizeof staging ? size - done : (uint32_t)sizeof staging;
        uint32_t sectors = (chunk + SECTOR_BYTES - 1) / SECTOR_BYTES;
        if (!emmc_read(NULL, first_sector + done / SECTOR_BYTES, sectors, staging)) {
            return false;
        }
        crc = image_crc_update(crc, (const uint8_t *)staging, chunk);
        for (uint32_t word = 0; word * WORD_BYTES < chunk; word++) {
            register_write(CORE1_CODE_BASE + done + word * WORD_BYTES, staging[word]);
        }
    }
    return crc == expected_crc;
}

static Core1Result place_program(Core1Program program)
{
    EmmcInfo info;
    if (hal_system_restart_pending() || !emmc_start(&info)) {
        return CORE1_NO_REPLY;
    }
    uint32_t offset = FLAC_PROGRAM_OFFSET;
    if (program == CORE1_PROGRAM_MP3) {
        offset = MP3_PROGRAM_OFFSET;
    } else if (program == CORE1_PROGRAM_AAC) {
        offset = AAC_PROGRAM_OFFSET;
    } else if (program == CORE1_PROGRAM_OPUS) {
        offset = OPUS_PROGRAM_OFFSET;
    }
    uint32_t header_sector = info.layout.slot_1 + offset / SECTOR_BYTES;
    if (!emmc_read(NULL, header_sector, 1, staging)) {
        return CORE1_NO_REPLY;
    }
    const uint8_t *header = (const uint8_t *)staging;
    uint32_t size = le32(header + 4);
    uint32_t crc = le32(header + 8);
    if (le32(header) != PROGRAM_MAGIC || size == 0 || size > CORE1_CODE_BYTES) {
        return CORE1_BAD_REPLY;
    }
    core1_hold(&BUS);
    if (!copy_program_sectors(header_sector + 1, size, crc)) {
        return CORE1_BAD_REPLY;
    }
    core1_prepare(&BUS);
    return CORE1_OK;
}

static bool equalizer_moved(void)
{
    for (int band = 0; band < equalizer_curve.band_count; band++) {
        if (equalizer_curve.bands[band].gain_db != 0) {
            return true;
        }
    }
    return false;
}

static bool equalizing(Core1Program program)
{
    return program == CORE1_PROGRAM_FLAC && equalizer_moved();
}

static uint32_t stream_divider(Core1Program program, uint32_t pll_mhz)
{
    bool heavy =
        program == CORE1_PROGRAM_AAC || program == CORE1_PROGRAM_OPUS || equalizing(program);
    return heavy && pll_mhz <= FULL_SPEED_MHZ ? 1 : core1_divider_for(pll_mhz);
}

static void share_equalizer(bool enabled)
{
    volatile Core1EqualizerShare *share =
        (volatile Core1EqualizerShare *)(uintptr_t)CORE1_SHARED_EQUALIZER;
    share->curve = equalizer_curve;
    share->enabled = enabled ? 1u : 0u;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    share->version = ++equalizer_version;
}

void board_core1_set_equalizer(const EqualizerCurve *curve)
{
    equalizer_curve = *curve;
    if (streaming && streaming_program == CORE1_PROGRAM_FLAC) {
        share_equalizer(streaming_divider == 1);
    }
}

bool board_core1_start_stream(Core1Program program)
{
    if (streaming && streaming_program == program &&
        streaming_divider == stream_divider(program, hal_system_clock_mhz())) {
        return true;
    }
    board_core1_stop();
    if (program == CORE1_PROGRAM_FLAC && !accel_known && hal_system_clock_mhz() >= LOWEST_PLL_MHZ) {
        (void)run_accel_test();
    }
    uint32_t pll_mhz = hal_system_clock_mhz();
    if (pll_mhz < LOWEST_PLL_MHZ) {
        return false;
    }
    board_start_record_step(HAL_START_STEP_CORE1_START);
    uint32_t divider = stream_divider(program, pll_mhz);
    Core1Result result = core1_power_up(&BUS, pll_mhz, divider);
    if (result == CORE1_OK) {
        result = place_program(program);
    }
    if (result == CORE1_OK) {
        register_write(CORE1_SHARED_COMMAND, CORE1_COMMAND_STREAM);
        register_write(CORE1_SHARED_ACCEL_CLASSES, accel_classes);
        share_equalizer(equalizing(program) && divider == 1);
        result = core1_run(&BUS, REPLY_TIMEOUT_MS);
    }
    board_start_record_step_done();
    if (result != CORE1_OK) {
        core1_stop(&BUS);
        return false;
    }
    streaming = true;
    streaming_program = program;
    streaming_divider = divider;
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
        result = place_program(CORE1_PROGRAM_FLAC);
    }
    if (result == CORE1_OK) {
        result = core1_run(&BUS, REPLY_TIMEOUT_MS);
    }
    if (result == CORE1_OK) {
        test.cycles = (HalLoopCycles){.register_centi = register_read(CORE1_SHARED_REGISTER_CENTI),
                                      .memory_centi = register_read(CORE1_SHARED_MEMORY_CENTI)};
    }
    Core1Result stopped = core1_stop(&BUS);
    test.status = status_of(result != CORE1_OK ? result : stopped);
    return test;
}

static bool streaming_music(void)
{
    const char *current = board_audio_current_path();
    return streaming && current != NULL && current[0] != '\0';
}

static HalCore1Accel accel_results(HalCore1Accel result)
{
    uint32_t status = register_read(CORE1_SHARED_ACCEL_STATUS);
    uint32_t failed = register_read(CORE1_SHARED_ACCEL_FAILED);
    uint32_t ticks_per_us = result.mhz == 0 ? 1 : result.mhz;
    result.narrow_ok = (status & CORE1_ACCEL_NARROW) != 0;
    result.wide_ok = (status & CORE1_ACCEL_WIDE) != 0;
    result.timed_out = (status & CORE1_ACCEL_TIMED_OUT) != 0;
    result.failed_vector = (uint8_t)(failed >> 16);
    result.failed_sample = failed & 0xFFFFu;
    result.hardware_us = register_read(CORE1_SHARED_ACCEL_HARDWARE_TICKS) / ticks_per_us;
    result.software_us = register_read(CORE1_SHARED_ACCEL_SOFTWARE_TICKS) / ticks_per_us;
    accel_known = true;
    accel_classes = status & (CORE1_ACCEL_NARROW | CORE1_ACCEL_WIDE);
    return result;
}

static HalCore1Accel run_accel_test(void)
{
    board_core1_stop();
    uint32_t pll_mhz = hal_system_clock_mhz();
    if (pll_mhz < LOWEST_PLL_MHZ) {
        return (HalCore1Accel){.status = HAL_CORE1_NEEDS_PLL};
    }
    uint32_t divider = core1_divider_for(pll_mhz);
    HalCore1Accel result = {.status = HAL_CORE1_RAN, .mhz = pll_mhz / divider};
    Core1Result ran = core1_power_up(&BUS, pll_mhz, divider);
    if (ran == CORE1_OK) {
        ran = place_program(CORE1_PROGRAM_FLAC);
    }
    if (ran == CORE1_OK) {
        register_write(CORE1_SHARED_COMMAND, CORE1_COMMAND_ACCEL);
        ran = core1_run(&BUS, DECODE_TIMEOUT_MS);
    }
    if (ran == CORE1_OK) {
        result = accel_results(result);
    }
    Core1Result stopped = core1_stop(&BUS);
    result.status = status_of(ran != CORE1_OK ? ran : stopped);
    return result;
}

HalCore1Accel hal_system_core1_accel_test(void)
{
    if (streaming_music()) {
        return (HalCore1Accel){.status = HAL_CORE1_BUSY};
    }
    return run_accel_test();
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

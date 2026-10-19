#include "diagnostics_values.h"

#include "core/string_builder.h"

const char *diagnostics_tone_result_text(HalToneResult result)
{
    switch (result) {
    case HAL_TONE_PLAYED:
        return "Played";
    case HAL_TONE_NO_DAC:
        return "No DAC";
    case HAL_TONE_SETUP_FAILED:
        return "Setup failed";
    case HAL_TONE_NOT_READY:
        return "Not ready";
    case HAL_TONE_STALLED:
        return "I2S stalled";
    case HAL_TONE_NO_DMAC:
        return "No DMAC";
    }
    return "";
}

static void append_hex(StringBuilder *builder, uint32_t value, int digits)
{
    static const char DIGITS[] = "0123456789ABCDEF";
    for (int digit = digits - 1; digit >= 0; digit--) {
        string_builder_append_char(builder, DIGITS[(value >> (4 * digit)) & 0xFu]);
    }
}

void diagnostics_tone_text(HalToneResult result, HalToneDetail detail, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    string_builder_append(&builder, diagnostics_tone_result_text(result));
    if (result == HAL_TONE_NO_DAC && detail.stage != 0) {
        string_builder_append(&builder, " s");
        string_builder_append_unsigned(&builder, detail.stage);
        string_builder_append_char(&builder, ' ');
        append_hex(&builder, detail.pending, 2);
    }
}

void diagnostics_boot_clocks_text(HalBootClocks clocks, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    append_hex(&builder, clocks.mode, 4);
    string_builder_append_char(&builder, ' ');
    append_hex(&builder, clocks.clock_select, 4);
    string_builder_append_char(&builder, ' ');
    append_hex(&builder, clocks.pclk_select, 4);
}

void diagnostics_dma_tone_text(DmaToneView view, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!view.tried) {
        return;
    }
    if (view.result != HAL_TONE_PLAYED) {
        diagnostics_tone_text(view.result, view.detail, text, capacity);
        return;
    }
    if (view.status.running) {
        string_builder_append(&builder, "Playing ");
        string_builder_append_unsigned(&builder, view.status.position);
        return;
    }
    string_builder_append(&builder, view.stopped ? "Stopped" : "Ended");
}

void diagnostics_boot_pll_text(HalBootClocks clocks, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    append_hex(&builder, clocks.pll_con0, 4);
    string_builder_append_char(&builder, ' ');
    append_hex(&builder, clocks.pll_con1, 4);
    string_builder_append_char(&builder, ' ');
    string_builder_append_unsigned(&builder, clocks.core_hz / 1000000u);
    string_builder_append(&builder, " MHz");
}

static const uint32_t CLOCK_STEPS_MHZ[] = {24, 96, 200};

uint32_t diagnostics_next_clock_mhz(uint32_t current_mhz)
{
    for (size_t index = 0; index < sizeof CLOCK_STEPS_MHZ / sizeof CLOCK_STEPS_MHZ[0]; index++) {
        if (CLOCK_STEPS_MHZ[index] > current_mhz) {
            return CLOCK_STEPS_MHZ[index];
        }
    }
    return CLOCK_STEPS_MHZ[0];
}

void diagnostics_clock_text(uint32_t mhz, bool failed, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (failed) {
        string_builder_append(&builder, "No lock");
        return;
    }
    string_builder_append_unsigned(&builder, mhz);
    string_builder_append(&builder, " MHz");
}

static void append_centi(StringBuilder *builder, const char *prefix, uint32_t centi)
{
    string_builder_append(builder, prefix);
    string_builder_append_unsigned(builder, centi / 100);
    string_builder_append_char(builder, '.');
    string_builder_append_char(builder, (char)('0' + centi / 10 % 10));
    string_builder_append_char(builder, (char)('0' + centi % 10));
}

void diagnostics_loop_text(bool run, HalLoopCycles cycles, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!run) {
        return;
    }
    append_centi(&builder, "r", cycles.register_centi);
    append_centi(&builder, " m", cycles.memory_centi);
}

static const char *core1_failure_text(HalCore1Status status)
{
    switch (status) {
    case HAL_CORE1_RAN:
        return "";
    case HAL_CORE1_UNAVAILABLE:
        return "Not here";
    case HAL_CORE1_NEEDS_PLL:
        return "Needs 96+ MHz";
    case HAL_CORE1_NO_POWER:
        return "No power";
    case HAL_CORE1_NO_REPLY:
        return "No reply";
    case HAL_CORE1_BAD_REPLY:
        return "Bad reply";
    case HAL_CORE1_NO_STOP:
        return "No stop";
    case HAL_CORE1_NO_TRACK:
        return "Play a track first";
    case HAL_CORE1_NOT_FLAC:
        return "Not FLAC";
    case HAL_CORE1_READ_FAILED:
        return "Read failed";
    case HAL_CORE1_BUSY:
        return "Busy with music";
    }
    return "";
}

static const char *step_name(HalStartStep step, bool finished)
{
    static const char *const OPEN_NAMES[HAL_START_STEP_COUNT] = {
        "",          "backlight on", "backlight off", "clock switch", "core 1 start", "core 1 stop",
        "card write"};
    static const char *const DONE_NAMES[HAL_START_STEP_COUNT] = {
        "",          "Backlight on", "Backlight off", "Clock switch", "Core 1 start", "Core 1 stop",
        "Card write"};
    return finished ? DONE_NAMES[step] : OPEN_NAMES[step];
}

void diagnostics_last_step_text(HalStartInfo info, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!info.warm || info.last_step == HAL_START_STEP_NONE ||
        info.last_step >= HAL_START_STEP_COUNT) {
        string_builder_append(&builder, "None");
        return;
    }
    if (!info.last_step_finished) {
        string_builder_append(&builder, "In ");
    }
    string_builder_append(&builder, step_name(info.last_step, info.last_step_finished));
    string_builder_append(&builder, " at ");
    string_builder_append_unsigned(&builder, info.last_step_ms / 1000u);
    string_builder_append(&builder, " s");
}

void diagnostics_last_battery_text(HalStartInfo info, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!info.warm || info.last_millivolts == 0) {
        string_builder_append(&builder, "No reading");
        return;
    }
    string_builder_append_unsigned(&builder, info.last_millivolts);
    string_builder_append(&builder, " mV");
}

void diagnostics_core1_text(bool run, HalCore1Test test, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!run) {
        return;
    }
    if (test.status != HAL_CORE1_RAN) {
        string_builder_append(&builder, core1_failure_text(test.status));
        return;
    }
    string_builder_append_unsigned(&builder, test.mhz);
    append_centi(&builder, " r", test.cycles.register_centi);
    append_centi(&builder, " m", test.cycles.memory_centi);
}

static uint32_t rounded_milliseconds(uint32_t microseconds)
{
    return (microseconds + 500u) / 1000u;
}

void diagnostics_core1_decode_text(bool run, HalCore1Decode result, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!run) {
        return;
    }
    if (result.status != HAL_CORE1_RAN) {
        string_builder_append(&builder, core1_failure_text(result.status));
        return;
    }
    if (result.blocks == 0) {
        string_builder_append(&builder, "No blocks");
        return;
    }
    string_builder_append_unsigned(&builder, result.mhz);
    string_builder_append(&builder, ": ");
    string_builder_append_unsigned(&builder, rounded_milliseconds(result.average_us));
    string_builder_append(&builder, "/");
    string_builder_append_unsigned(&builder, rounded_milliseconds(result.block_us));
    string_builder_append(&builder, " ms max ");
    string_builder_append_unsigned(&builder, rounded_milliseconds(result.longest_us));
    if (result.lpc_in_hardware > 0) {
        string_builder_append(&builder, " hw");
    }
}

static void append_tenths(StringBuilder *builder, uint32_t microseconds)
{
    uint32_t tenths = (microseconds + 50u) / 100u;
    string_builder_append_unsigned(builder, tenths / 10u);
    string_builder_append_char(builder, '.');
    string_builder_append_unsigned(builder, tenths % 10u);
}

void diagnostics_core1_accel_text(bool run, HalCore1Accel result, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!run) {
        return;
    }
    if (result.status != HAL_CORE1_RAN) {
        string_builder_append(&builder, core1_failure_text(result.status));
        return;
    }
    if (result.timed_out) {
        string_builder_append(&builder, "Timeout at t");
        string_builder_append_unsigned(&builder, result.failed_vector);
        return;
    }
    string_builder_append(&builder, result.narrow_ok ? "N ok" : "N no");
    string_builder_append(&builder, result.wide_ok ? " W ok" : " W no");
    if (result.failed_vector > 0) {
        string_builder_append(&builder, " t");
        string_builder_append_unsigned(&builder, result.failed_vector);
        string_builder_append_char(&builder, '@');
        string_builder_append_unsigned(&builder, result.failed_sample);
        return;
    }
    string_builder_append_char(&builder, ' ');
    append_tenths(&builder, result.hardware_us);
    string_builder_append_char(&builder, '/');
    append_tenths(&builder, result.software_us);
    string_builder_append(&builder, " ms");
}

void diagnostics_battery_text(HalBattery battery, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!battery.known) {
        string_builder_append(&builder, "No reading");
        return;
    }
    string_builder_append_unsigned(&builder, battery.millivolts);
    string_builder_append(&builder, " mV");
    if (battery.usb_power) {
        string_builder_append(&builder, battery.charging ? " charging" : " full");
    }
}

void diagnostics_boot_hold_text(HalPowerHold hold, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    string_builder_append(&builder, "fn");
    string_builder_append_unsigned(&builder, hold.function);
    string_builder_append(&builder, hold.output ? " output" : " input");
    string_builder_append(&builder, hold.high ? " high" : " low");
}

void diagnostics_start_text(HalStartInfo info, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!info.warm) {
        string_builder_append(&builder, "Cold start");
        return;
    }
    if (info.fault_status != 0 || info.hard_fault_status != 0) {
        string_builder_append(&builder, "Fault at ");
        append_hex(&builder, info.fault_pc, 8);
        return;
    }
    string_builder_append_char(&builder, '#');
    string_builder_append_unsigned(&builder, info.starts);
    if (info.previous_power_off) {
        string_builder_append(&builder, " after power-off");
        return;
    }
    string_builder_append(&builder, " reset after ");
    string_builder_append_unsigned(&builder, info.previous_uptime_ms / 1000u);
    string_builder_append(&builder, " s");
}

void diagnostics_high_ram_text(bool run, HalHighRamTest test, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!run) {
        return;
    }
    switch (test.status) {
    case HAL_HIGH_RAM_OK:
        string_builder_append(&builder, "OK ");
        string_builder_append_unsigned(&builder, test.size / 1024u);
        string_builder_append(&builder, " KB, call works");
        return;
    case HAL_HIGH_RAM_UNAVAILABLE:
        string_builder_append(&builder, "Not on this player");
        return;
    case HAL_HIGH_RAM_BAD_MEMORY:
        string_builder_append(&builder, "Bad at ");
        append_hex(&builder, test.bad_address, 8);
        return;
    case HAL_HIGH_RAM_BAD_CALL:
        string_builder_append(&builder, "Call gave ");
        append_hex(&builder, test.call_result, 8);
        return;
    }
}

void diagnostics_jack_pin_text(HalJackPin pin, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    string_builder_append(&builder, pin.high ? "high, boot fn" : "low, boot fn");
    string_builder_append_unsigned(&builder, pin.boot_function);
    string_builder_append(&builder, pin.boot_output ? " out" : " in");
    if (pin.pull) {
        string_builder_append(&builder, " pull");
    }
}

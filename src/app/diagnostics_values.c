#include "diagnostics_values.h"

#include "core/i18n.h"
#include "core/string_builder.h"

enum {
    ARGUMENT_CAPACITY = 12
};

const char *diagnostics_tone_result_text(HalToneResult result)
{
    switch (result) {
    case HAL_TONE_PLAYED:
        return i18n_text(STR_DIAG_TONE_PLAYED);
    case HAL_TONE_NO_DAC:
        return i18n_text(STR_DIAG_TONE_NO_DAC);
    case HAL_TONE_SETUP_FAILED:
        return i18n_text(STR_DIAG_TONE_SETUP_FAILED);
    case HAL_TONE_NOT_READY:
        return i18n_text(STR_DIAG_TONE_NOT_READY);
    case HAL_TONE_STALLED:
        return i18n_text(STR_DIAG_TONE_STALLED);
    case HAL_TONE_NO_DMAC:
        return i18n_text(STR_DIAG_TONE_NO_DMAC);
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

static void number_text(char *text, size_t capacity, uint32_t value)
{
    StringBuilder builder = string_builder_make(text, capacity);
    string_builder_append_unsigned(&builder, value);
}

void diagnostics_panel_text(uint32_t id, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    append_hex(&builder, id, 6);
}

static void hex_text(char *text, size_t capacity, uint32_t value, int digits)
{
    StringBuilder builder = string_builder_make(text, capacity);
    append_hex(&builder, value, digits);
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

void diagnostics_usb_registers_text(bool captured, uint32_t first, uint32_t second, char *text,
                                    size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!captured) {
        string_builder_append(&builder, i18n_text(STR_DIAG_PLUG_IN_USB));
        return;
    }
    append_hex(&builder, first, 8);
    string_builder_append_char(&builder, ' ');
    append_hex(&builder, second, 8);
}

static uint32_t drives_in(uint8_t mask)
{
    return (uint32_t)(mask & 1u) + (uint32_t)((mask >> 1) & 1u);
}

void diagnostics_usb_last_text(HalUsbLast last, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (last.end == HAL_USB_END_NONE) {
        string_builder_append(&builder, i18n_text(STR_DIAG_USB_NO_SESSION));
        return;
    }
    if (last.end == HAL_USB_END_NO_HOST) {
        string_builder_append(&builder, i18n_text(STR_DIAG_USB_NO_COMPUTER));
        return;
    }
    char internal[ARGUMENT_CAPACITY];
    char card[ARGUMENT_CAPACITY];
    number_text(internal, sizeof internal, last.internal_written);
    number_text(card, sizeof card, last.card_written);
    if (last.end == HAL_USB_END_EJECTED) {
        const char *arguments[] = {internal, card};
        i18n_format(&builder, STR_DIAG_USB_EJECTED, arguments, 2);
    } else {
        char drives[ARGUMENT_CAPACITY];
        number_text(drives, sizeof drives, drives_in(last.left_mounted));
        const char *arguments[] = {drives, internal, card};
        i18n_format(&builder, STR_DIAG_USB_UNPLUGGED, arguments, 3);
    }
    if (last.writes_after_eject > 0) {
        string_builder_append(&builder, " A");
        string_builder_append_unsigned(&builder, last.writes_after_eject);
    }
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
        char position[ARGUMENT_CAPACITY];
        number_text(position, sizeof position, view.status.position);
        const char *arguments[] = {position};
        i18n_format(&builder, STR_DIAG_TONE_PLAYING, arguments, 1);
        return;
    }
    string_builder_append(&builder,
                          i18n_text(view.stopped ? STR_DIAG_TONE_STOPPED : STR_DIAG_TONE_ENDED));
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
        string_builder_append(&builder, i18n_text(STR_DIAG_NO_LOCK));
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
        return i18n_text(STR_DIAG_CORE1_NOT_HERE);
    case HAL_CORE1_NEEDS_PLL:
        return i18n_text(STR_DIAG_CORE1_NEEDS_PLL);
    case HAL_CORE1_NO_POWER:
        return i18n_text(STR_DIAG_CORE1_NO_POWER);
    case HAL_CORE1_NO_REPLY:
        return i18n_text(STR_DIAG_NO_REPLY);
    case HAL_CORE1_BAD_REPLY:
        return i18n_text(STR_DIAG_CORE1_BAD_REPLY);
    case HAL_CORE1_NO_STOP:
        return i18n_text(STR_DIAG_CORE1_NO_STOP);
    case HAL_CORE1_BUSY:
        return i18n_text(STR_DIAG_CORE1_BUSY);
    }
    return "";
}

static const char *step_name(HalStartStep step, bool finished)
{
    static const StringId OPEN_NAMES[HAL_START_STEP_COUNT] = {STR_DIAG_NONE,
                                                              STR_DIAG_STEP_BACKLIGHT_ON_OPEN,
                                                              STR_DIAG_STEP_BACKLIGHT_OFF_OPEN,
                                                              STR_DIAG_STEP_CLOCK_SWITCH_OPEN,
                                                              STR_DIAG_STEP_CORE1_START_OPEN,
                                                              STR_DIAG_STEP_CORE1_STOP_OPEN,
                                                              STR_DIAG_STEP_CARD_WRITE_OPEN,
                                                              STR_DIAG_STEP_WATCHDOG_OPEN};
    static const StringId DONE_NAMES[HAL_START_STEP_COUNT] = {STR_DIAG_NONE,
                                                              STR_DIAG_STEP_BACKLIGHT_ON_DONE,
                                                              STR_DIAG_STEP_BACKLIGHT_OFF_DONE,
                                                              STR_DIAG_STEP_CLOCK_SWITCH_DONE,
                                                              STR_DIAG_STEP_CORE1_START_DONE,
                                                              STR_DIAG_STEP_CORE1_STOP_DONE,
                                                              STR_DIAG_STEP_CARD_WRITE_DONE,
                                                              STR_DIAG_STEP_WATCHDOG_DONE};
    if (step == HAL_START_STEP_NONE) {
        return "";
    }
    return i18n_text(finished ? DONE_NAMES[step] : OPEN_NAMES[step]);
}

void diagnostics_last_step_text(HalStartInfo info, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!info.warm || info.last_step == HAL_START_STEP_NONE ||
        info.last_step >= HAL_START_STEP_COUNT) {
        string_builder_append(&builder, i18n_text(STR_DIAG_NONE));
        return;
    }
    char seconds[ARGUMENT_CAPACITY];
    number_text(seconds, sizeof seconds, info.last_step_ms / 1000u);
    const char *arguments[] = {step_name(info.last_step, info.last_step_finished), seconds};
    i18n_format(&builder, info.last_step_finished ? STR_DIAG_STEP_FINISHED : STR_DIAG_STEP_IN,
                arguments, 2);
}

void diagnostics_last_battery_text(HalStartInfo info, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!info.warm || info.last_millivolts == 0) {
        string_builder_append(&builder, i18n_text(STR_DIAG_NO_READING));
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
        char vector[ARGUMENT_CAPACITY];
        number_text(vector, sizeof vector, result.failed_vector);
        const char *arguments[] = {vector};
        i18n_format(&builder, STR_DIAG_CORE1_TIMEOUT, arguments, 1);
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
        string_builder_append(&builder, i18n_text(STR_DIAG_NO_READING));
        return;
    }
    if (battery.usb_power) {
        char millivolts[ARGUMENT_CAPACITY];
        number_text(millivolts, sizeof millivolts, battery.millivolts);
        const char *arguments[] = {millivolts};
        i18n_format(&builder, battery.charging ? STR_DIAG_BATTERY_CHARGING : STR_DIAG_BATTERY_FULL,
                    arguments, 1);
        return;
    }
    string_builder_append_unsigned(&builder, battery.millivolts);
    string_builder_append(&builder, " mV");
}

void diagnostics_boot_hold_text(HalPowerHold hold, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    string_builder_append(&builder, "fn");
    string_builder_append_unsigned(&builder, hold.function);
    string_builder_append(&builder, hold.output ? " output" : " input");
    string_builder_append(&builder, hold.high ? " high" : " low");
    if (hold.usb_power) {
        string_builder_append(&builder, " · USB");
    }
    if (hold.key_down) {
        string_builder_append(&builder, " · key");
    }
}

void diagnostics_start_text(HalStartInfo info, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!info.warm) {
        string_builder_append(&builder, i18n_text(STR_DIAG_COLD_START));
        return;
    }
    if (info.fault_status != 0 || info.hard_fault_status != 0) {
        char address[ARGUMENT_CAPACITY];
        hex_text(address, sizeof address, info.fault_pc, 8);
        const char *arguments[] = {address};
        i18n_format(&builder, STR_DIAG_FAULT_AT, arguments, 1);
        return;
    }
    char starts[ARGUMENT_CAPACITY];
    char seconds[ARGUMENT_CAPACITY];
    number_text(starts, sizeof starts, info.starts);
    number_text(seconds, sizeof seconds, info.previous_uptime_ms / 1000u);
    const char *arguments[] = {starts, seconds};
    i18n_format(&builder,
                info.previous_power_off ? STR_DIAG_START_AFTER_POWER_OFF
                                        : STR_DIAG_START_RESET_AFTER,
                arguments, 2);
}

void diagnostics_high_ram_text(bool run, HalHighRamTest test, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!run) {
        return;
    }
    char argument[ARGUMENT_CAPACITY];
    const char *arguments[] = {argument};
    switch (test.status) {
    case HAL_HIGH_RAM_OK:
        number_text(argument, sizeof argument, test.size / 1024u);
        i18n_format(&builder, STR_DIAG_RAM_OK, arguments, 1);
        return;
    case HAL_HIGH_RAM_UNAVAILABLE:
        string_builder_append(&builder, i18n_text(STR_DIAG_RAM_UNAVAILABLE));
        return;
    case HAL_HIGH_RAM_BAD_MEMORY:
        hex_text(argument, sizeof argument, test.bad_address, 8);
        i18n_format(&builder, STR_DIAG_RAM_BAD_AT, arguments, 1);
        return;
    case HAL_HIGH_RAM_BAD_CALL:
        hex_text(argument, sizeof argument, test.call_result, 8);
        i18n_format(&builder, STR_DIAG_RAM_CALL_GAVE, arguments, 1);
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

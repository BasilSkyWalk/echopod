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

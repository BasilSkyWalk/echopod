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

void diagnostics_dma_tone_text(DmaToneView view, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!view.tried) {
        return;
    }
    if (view.result != HAL_TONE_PLAYED) {
        string_builder_append(&builder, diagnostics_tone_result_text(view.result));
        return;
    }
    if (view.status.running) {
        string_builder_append(&builder, "Playing ");
        string_builder_append_unsigned(&builder, view.status.position);
        return;
    }
    string_builder_append(&builder, view.stopped ? "Stopped" : "Ended");
}

#include <string.h>

#include "check.h"
#include "diagnostics_values.h"

static bool text_is(const char *expected, const char *actual)
{
    return strcmp(expected, actual) == 0;
}

static void an_untried_tone_shows_nothing(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    DmaToneView view = {.tried = false};
    diagnostics_dma_tone_text(view, text, sizeof text);
    CHECK(text_is("", text));
}

static void a_running_tone_shows_the_read_position(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    DmaToneView view = {
        .tried = true, .result = HAL_TONE_PLAYED, .status = {.running = true, .position = 1234}};
    diagnostics_dma_tone_text(view, text, sizeof text);
    CHECK(text_is("Playing 1234", text));
}

static void a_tone_that_ended_by_itself_says_so(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    DmaToneView view = {.tried = true, .result = HAL_TONE_PLAYED, .status = {.running = false}};
    diagnostics_dma_tone_text(view, text, sizeof text);
    CHECK(text_is("Ended", text));
    view.stopped = true;
    diagnostics_dma_tone_text(view, text, sizeof text);
    CHECK(text_is("Stopped", text));
}

static void a_failed_start_names_the_step(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    DmaToneView view = {.tried = true, .result = HAL_TONE_NO_DMAC};
    diagnostics_dma_tone_text(view, text, sizeof text);
    CHECK(text_is("No DMAC", text));
    view.result = HAL_TONE_NO_DAC;
    diagnostics_dma_tone_text(view, text, sizeof text);
    CHECK(text_is("No DAC", text));
}

static void a_missing_dac_names_the_i2c_step_and_status(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    diagnostics_tone_text(HAL_TONE_NO_DAC, (HalToneDetail){.stage = 3, .pending = 0x48}, text,
                          sizeof text);
    CHECK(text_is("No DAC s3 48", text));
    diagnostics_tone_text(HAL_TONE_PLAYED, (HalToneDetail){.stage = 0, .pending = 0}, text,
                          sizeof text);
    CHECK(text_is("Played", text));
}

static void boot_clocks_show_the_low_half_of_each_register(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    HalBootClocks clocks = {
        .mode = 0x00200001u, .clock_select = 0x00001023u, .pclk_select = 0x0100u};
    diagnostics_boot_clocks_text(clocks, text, sizeof text);
    CHECK(text_is("0001 1023 0100", text));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(an_untried_tone_shows_nothing),
        TEST_CASE(a_running_tone_shows_the_read_position),
        TEST_CASE(a_tone_that_ended_by_itself_says_so),
        TEST_CASE(a_failed_start_names_the_step),
        TEST_CASE(a_missing_dac_names_the_i2c_step_and_status),
        TEST_CASE(boot_clocks_show_the_low_half_of_each_register),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

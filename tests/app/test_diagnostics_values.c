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

static void the_boot_pll_shows_its_fields_and_core_speed(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    HalBootClocks clocks = {.pll_con0 = 0xFFFF1078u, .pll_con1 = 0x1546u, .core_hz = 96000000u};
    diagnostics_boot_pll_text(clocks, text, sizeof text);
    CHECK(text_is("1078 1546 96 MHz", text));
}

static void the_clock_row_steps_up_from_the_current_speed_then_wraps(void)
{
    CHECK_EQUAL(96u, diagnostics_next_clock_mhz(48));
    CHECK_EQUAL(96u, diagnostics_next_clock_mhz(24));
    CHECK_EQUAL(200u, diagnostics_next_clock_mhz(96));
    CHECK_EQUAL(24u, diagnostics_next_clock_mhz(200));
}

static void the_clock_row_shows_the_actual_speed(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    diagnostics_clock_text(48, false, text, sizeof text);
    CHECK(text_is("48 MHz", text));
    diagnostics_clock_text(48, true, text, sizeof text);
    CHECK(text_is("No lock", text));
}

static void the_loop_row_is_empty_until_run(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    diagnostics_loop_text(false, (HalLoopCycles){0}, text, sizeof text);
    CHECK(text_is("", text));
}

static void the_loop_row_shows_cycles_per_iteration(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    diagnostics_loop_text(true, (HalLoopCycles){.register_centi = 302, .memory_centi = 1507}, text,
                          sizeof text);
    CHECK(text_is("r3.02 m15.07", text));
    diagnostics_loop_text(true, (HalLoopCycles){.register_centi = 400, .memory_centi = 5}, text,
                          sizeof text);
    CHECK(text_is("r4.00 m0.05", text));
}

static void the_core_1_row_is_empty_until_run(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    diagnostics_core1_text(false, (HalCore1Test){0}, text, sizeof text);
    CHECK(text_is("", text));
}

static void a_core_1_run_shows_its_clock_and_loop_cycles(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    HalCore1Test test = {.status = HAL_CORE1_RAN,
                         .mhz = 100,
                         .cycles = {.register_centi = 300, .memory_centi = 912}};
    diagnostics_core1_text(true, test, text, sizeof text);
    CHECK(text_is("100 r3.00 m9.12", text));
}

static void core_1_failures_name_the_step(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    static const struct {
        HalCore1Status status;
        const char *text;
    } cases[] = {
        {HAL_CORE1_UNAVAILABLE, "Not here"}, {HAL_CORE1_NEEDS_PLL, "Needs 96+ MHz"},
        {HAL_CORE1_NO_POWER, "No power"},    {HAL_CORE1_NO_REPLY, "No reply"},
        {HAL_CORE1_BAD_REPLY, "Bad reply"},  {HAL_CORE1_NO_STOP, "No stop"},
        {HAL_CORE1_BUSY, "Busy with music"},
    };
    for (size_t index = 0; index < sizeof cases / sizeof cases[0]; index++) {
        diagnostics_core1_text(true, (HalCore1Test){.status = cases[index].status}, text,
                               sizeof text);
        CHECK(text_is(cases[index].text, text));
    }
}

static void the_core_1_decode_row_shows_average_block_time_against_real_time(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    HalCore1Decode result = {.status = HAL_CORE1_RAN,
                             .mhz = 100,
                             .blocks = 12,
                             .average_us = 31400,
                             .longest_us = 40600,
                             .block_us = 48000};
    diagnostics_core1_decode_text(true, result, text, sizeof text);
    CHECK(text_is("100: 31/48 ms max 41", text));
    diagnostics_core1_decode_text(false, result, text, sizeof text);
    CHECK(text_is("", text));
}

static void the_accelerator_row_shows_both_classes_and_the_timing(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    diagnostics_core1_accel_text(false, (HalCore1Accel){0}, text, sizeof text);
    CHECK(text_is("", text));
    diagnostics_core1_accel_text(true,
                                 (HalCore1Accel){.status = HAL_CORE1_RAN,
                                                 .narrow_ok = true,
                                                 .wide_ok = true,
                                                 .hardware_us = 1230,
                                                 .software_us = 9760},
                                 text, sizeof text);
    CHECK(text_is("N ok W ok 1.2/9.8 ms", text));
    diagnostics_core1_accel_text(
        true,
        (HalCore1Accel){
            .status = HAL_CORE1_RAN, .narrow_ok = true, .failed_vector = 9, .failed_sample = 123},
        text, sizeof text);
    CHECK(text_is("N ok W no t9@123", text));
    diagnostics_core1_accel_text(
        true, (HalCore1Accel){.status = HAL_CORE1_RAN, .timed_out = true, .failed_vector = 5}, text,
        sizeof text);
    CHECK(text_is("Timeout at t5", text));
    diagnostics_core1_accel_text(true, (HalCore1Accel){.status = HAL_CORE1_NO_REPLY}, text,
                                 sizeof text);
    CHECK(text_is("No reply", text));
}

static void a_decode_that_used_the_accelerator_says_so(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    diagnostics_core1_decode_text(true,
                                  (HalCore1Decode){.status = HAL_CORE1_RAN,
                                                   .mhz = 100,
                                                   .blocks = 10,
                                                   .average_us = 18200,
                                                   .longest_us = 21000,
                                                   .block_us = 24000,
                                                   .lpc_in_hardware = 40},
                                  text, sizeof text);
    CHECK(text_is("100: 18/24 ms max 21 hw", text));
}

static void a_core_1_decode_without_blocks_or_track_says_so(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    diagnostics_core1_decode_text(true, (HalCore1Decode){.status = HAL_CORE1_RAN, .mhz = 100}, text,
                                  sizeof text);
    CHECK(text_is("No blocks", text));
    diagnostics_core1_decode_text(true, (HalCore1Decode){.status = HAL_CORE1_NO_TRACK}, text,
                                  sizeof text);
    CHECK(text_is("Play a track first", text));
    diagnostics_core1_decode_text(true, (HalCore1Decode){.status = HAL_CORE1_NOT_FLAC}, text,
                                  sizeof text);
    CHECK(text_is("Not FLAC", text));
    diagnostics_core1_decode_text(true, (HalCore1Decode){.status = HAL_CORE1_READ_FAILED}, text,
                                  sizeof text);
    CHECK(text_is("Read failed", text));
    diagnostics_core1_decode_text(true, (HalCore1Decode){.status = HAL_CORE1_NO_REPLY}, text,
                                  sizeof text);
    CHECK(text_is("No reply", text));
}

static void the_battery_row_shows_millivolts_and_the_charger_state(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    diagnostics_battery_text((HalBattery){0}, text, sizeof text);
    CHECK(text_is("No reading", text));
    HalBattery battery = {.known = true, .percent = 50, .millivolts = 3803};
    diagnostics_battery_text(battery, text, sizeof text);
    CHECK(text_is("3803 mV", text));
    battery.usb_power = true;
    battery.charging = true;
    diagnostics_battery_text(battery, text, sizeof text);
    CHECK(text_is("3803 mV charging", text));
    battery.charging = false;
    diagnostics_battery_text(battery, text, sizeof text);
    CHECK(text_is("3803 mV full", text));
}

static void the_boot_hold_row_shows_how_the_rom_left_the_pin(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    diagnostics_boot_hold_text((HalPowerHold){.function = 0, .output = false, .high = true}, text,
                               sizeof text);
    CHECK(text_is("fn0 input high", text));
    diagnostics_boot_hold_text((HalPowerHold){.function = 2, .output = true, .high = false}, text,
                               sizeof text);
    CHECK(text_is("fn2 output low", text));
}

static void a_jack_row_shows_the_live_level_and_how_the_rom_left_the_pin(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    diagnostics_jack_pin_text(
        (HalJackPin){.high = true, .boot_function = 0, .boot_output = false, .pull = true}, text,
        sizeof text);
    CHECK(text_is("high, boot fn0 in pull", text));
    diagnostics_jack_pin_text(
        (HalJackPin){.high = false, .boot_function = 1, .boot_output = true, .pull = false}, text,
        sizeof text);
    CHECK(text_is("low, boot fn1 out", text));
}

static void the_last_start_row_tells_cold_starts_resets_and_faults_apart(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    diagnostics_start_text((HalStartInfo){.warm = false, .starts = 1}, text, sizeof text);
    CHECK(text_is("Cold start", text));
    diagnostics_start_text((HalStartInfo){.warm = true, .starts = 3, .previous_uptime_ms = 61400},
                           text, sizeof text);
    CHECK(text_is("#3 reset after 61 s", text));
    diagnostics_start_text((HalStartInfo){.warm = true, .starts = 2, .previous_power_off = true},
                           text, sizeof text);
    CHECK(text_is("#2 after power-off", text));
    diagnostics_start_text(
        (HalStartInfo){
            .warm = true, .starts = 2, .fault_status = 0x00020000u, .fault_pc = 0x0305ABCDu},
        text, sizeof text);
    CHECK(text_is("Fault at 0305ABCD", text));
}

static void the_last_step_row_tells_an_unfinished_step_from_a_finished_one(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    diagnostics_last_step_text((HalStartInfo){.warm = false}, text, sizeof text);
    CHECK(text_is("None", text));
    diagnostics_last_step_text((HalStartInfo){.warm = true, .last_step = HAL_START_STEP_NONE}, text,
                               sizeof text);
    CHECK(text_is("None", text));
    diagnostics_last_step_text((HalStartInfo){.warm = true,
                                              .last_step = HAL_START_STEP_CLOCK_SWITCH,
                                              .last_step_ms = 41900},
                               text, sizeof text);
    CHECK(text_is("In clock switch at 41 s", text));
    diagnostics_last_step_text((HalStartInfo){.warm = true,
                                              .last_step = HAL_START_STEP_BACKLIGHT_OFF,
                                              .last_step_finished = true,
                                              .last_step_ms = 12345000},
                               text, sizeof text);
    CHECK(text_is("Backlight off at 12345 s", text));
}

static void the_last_battery_row_shows_the_reading_before_the_start(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    diagnostics_last_battery_text((HalStartInfo){.warm = false}, text, sizeof text);
    CHECK(text_is("No reading", text));
    diagnostics_last_battery_text((HalStartInfo){.warm = true, .last_millivolts = 3712}, text,
                                  sizeof text);
    CHECK(text_is("3712 mV", text));
}

static void the_high_ram_row_reports_each_outcome(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    diagnostics_high_ram_text(false, (HalHighRamTest){.status = HAL_HIGH_RAM_OK}, text,
                              sizeof text);
    CHECK(text_is("", text));
    diagnostics_high_ram_text(true, (HalHighRamTest){.status = HAL_HIGH_RAM_OK, .size = 151552},
                              text, sizeof text);
    CHECK(text_is("OK 148 KB, call works", text));
    diagnostics_high_ram_text(true, (HalHighRamTest){.status = HAL_HIGH_RAM_UNAVAILABLE}, text,
                              sizeof text);
    CHECK(text_is("Not on this player", text));
    diagnostics_high_ram_text(
        true, (HalHighRamTest){.status = HAL_HIGH_RAM_BAD_MEMORY, .bad_address = 0x0307A010u}, text,
        sizeof text);
    CHECK(text_is("Bad at 0307A010", text));
    diagnostics_high_ram_text(
        true, (HalHighRamTest){.status = HAL_HIGH_RAM_BAD_CALL, .call_result = 0x12u}, text,
        sizeof text);
    CHECK(text_is("Call gave 00000012", text));
}

static void the_watchdog_test_is_named_in_the_last_step_row(void)
{
    char text[DIAGNOSTICS_VALUE_CAPACITY];
    diagnostics_last_step_text((HalStartInfo){.warm = true,
                                              .last_step = HAL_START_STEP_WATCHDOG_TEST,
                                              .last_step_finished = false,
                                              .last_step_ms = 12000},
                               text, sizeof text);
    CHECK(strcmp("In watchdog test at 12 s", text) == 0);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_watchdog_test_is_named_in_the_last_step_row),
        TEST_CASE(an_untried_tone_shows_nothing),
        TEST_CASE(the_loop_row_is_empty_until_run),
        TEST_CASE(the_last_start_row_tells_cold_starts_resets_and_faults_apart),
        TEST_CASE(the_last_step_row_tells_an_unfinished_step_from_a_finished_one),
        TEST_CASE(the_last_battery_row_shows_the_reading_before_the_start),
        TEST_CASE(the_high_ram_row_reports_each_outcome),
        TEST_CASE(the_boot_hold_row_shows_how_the_rom_left_the_pin),
        TEST_CASE(a_jack_row_shows_the_live_level_and_how_the_rom_left_the_pin),
        TEST_CASE(the_battery_row_shows_millivolts_and_the_charger_state),
        TEST_CASE(the_core_1_row_is_empty_until_run),
        TEST_CASE(a_core_1_run_shows_its_clock_and_loop_cycles),
        TEST_CASE(core_1_failures_name_the_step),
        TEST_CASE(the_core_1_decode_row_shows_average_block_time_against_real_time),
        TEST_CASE(a_core_1_decode_without_blocks_or_track_says_so),
        TEST_CASE(the_accelerator_row_shows_both_classes_and_the_timing),
        TEST_CASE(a_decode_that_used_the_accelerator_says_so),
        TEST_CASE(the_loop_row_shows_cycles_per_iteration),
        TEST_CASE(a_running_tone_shows_the_read_position),
        TEST_CASE(a_tone_that_ended_by_itself_says_so),
        TEST_CASE(a_failed_start_names_the_step),
        TEST_CASE(a_missing_dac_names_the_i2c_step_and_status),
        TEST_CASE(boot_clocks_show_the_low_half_of_each_register),
        TEST_CASE(the_boot_pll_shows_its_fields_and_core_speed),
        TEST_CASE(the_clock_row_steps_up_from_the_current_speed_then_wraps),
        TEST_CASE(the_clock_row_shows_the_actual_speed),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

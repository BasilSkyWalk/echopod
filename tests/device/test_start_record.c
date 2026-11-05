#include "check.h"
#include "device/start_record.h"

static StartRecord garbage(void)
{
    return (StartRecord){.magic = 0x12345678u, .starts = 99, .check = 7, .uptime_ms = 5000};
}

static StallSnapshot reddit_stall(void)
{
    return (StallSnapshot){.pc = 0x03051994u,
                           .lr = 0x03053FC3u,
                           .sp = 0x03003D80u,
                           .words = {0x03051995u, 0x0307A010u, 0x00000007u},
                           .stack_used = 2740};
}

static void a_record_without_its_magic_reads_as_a_cold_start(void)
{
    StartRecord record = garbage();
    StartInfo info = start_record_boot(&record);
    CHECK(!info.warm);
    CHECK_EQUAL(1u, record.starts);
    CHECK_EQUAL(0u, record.uptime_ms);
    CHECK_EQUAL(0u, record.fault_status);
}

static void a_surviving_record_reads_as_a_warm_start_with_the_last_uptime(void)
{
    StartRecord record = garbage();
    start_record_boot(&record);
    start_record_tick(&record, 61000);
    StartInfo info = start_record_boot(&record);
    CHECK(info.warm);
    CHECK_EQUAL(2u, info.starts);
    CHECK_EQUAL(61000u, info.previous_uptime_ms);
    CHECK_EQUAL(0u, record.uptime_ms);
}

static void a_corrupted_count_reads_as_cold(void)
{
    StartRecord record = garbage();
    start_record_boot(&record);
    record.starts = 40;
    StartInfo info = start_record_boot(&record);
    CHECK(!info.warm);
    CHECK_EQUAL(1u, record.starts);
}

static void a_fault_survives_into_the_next_start(void)
{
    StartRecord record = garbage();
    start_record_boot(&record);
    start_record_fault(&record, 0x00020000u, 0x40000000u, 0x0305ABCDu);
    StartInfo info = start_record_boot(&record);
    CHECK(info.warm);
    CHECK_EQUAL(0x00020000u, info.fault_status);
    CHECK_EQUAL(0x40000000u, info.hard_fault_status);
    CHECK_EQUAL(0x0305ABCDu, info.fault_address);
    CHECK_EQUAL(0u, record.fault_status);
}

static void a_stall_survives_into_the_next_start_with_its_pc_and_lr(void)
{
    StartRecord record = garbage();
    StallSnapshot stall = reddit_stall();
    start_record_boot(&record);
    start_record_tick(&record, 69000);
    start_record_stall(&record, &stall);
    StartInfo info = start_record_boot(&record);
    CHECK(info.warm);
    CHECK(info.stalled);
    CHECK_EQUAL(0x03051994u, info.fault_address);
    CHECK_EQUAL(0x03053FC3u, info.stall_lr);
    CHECK_EQUAL(0u, info.fault_status);
    CHECK_EQUAL(69000u, info.previous_uptime_ms);
    CHECK_EQUAL(0u, record.stalled);
    CHECK_EQUAL(0u, record.stall_lr);
    CHECK_EQUAL(0u, record.fault_address);
}

static void a_stall_keeps_the_stack_pointer_the_words_above_the_frame_and_the_depth(void)
{
    StartRecord record = garbage();
    StallSnapshot stall = reddit_stall();
    start_record_boot(&record);
    start_record_stall(&record, &stall);
    StartInfo info = start_record_boot(&record);
    CHECK_EQUAL(0x03003D80u, info.stall_sp);
    CHECK_EQUAL(0x03051995u, info.stall_words[0]);
    CHECK_EQUAL(0x0307A010u, info.stall_words[1]);
    CHECK_EQUAL(7u, info.stall_words[2]);
    CHECK_EQUAL(2740u, info.stall_stack_used);
    CHECK_EQUAL(0u, record.stall_sp);
    CHECK_EQUAL(0u, record.stall_words[0]);
    CHECK_EQUAL(0u, record.stall_stack_used);
}

static void a_run_without_a_stall_reports_none(void)
{
    StartRecord record = garbage();
    record.stalled = 1;
    record.stall_lr = 0x03053FC3u;
    StartInfo cold = start_record_boot(&record);
    CHECK(!cold.stalled);
    CHECK_EQUAL(0u, cold.stall_lr);
    StartInfo warm = start_record_boot(&record);
    CHECK(warm.warm);
    CHECK(!warm.stalled);
    CHECK_EQUAL(0u, warm.stall_sp);
}

static void a_clean_power_off_is_remembered(void)
{
    StartRecord record = garbage();
    start_record_boot(&record);
    start_record_power_off(&record);
    StartInfo info = start_record_boot(&record);
    CHECK(info.warm);
    CHECK(info.previous_power_off);
}

static void the_last_step_survives_with_its_start_time_and_whether_it_finished(void)
{
    StartRecord record = garbage();
    start_record_boot(&record);
    start_record_tick(&record, 41000);
    start_record_step_begin(&record, HAL_START_STEP_CLOCK_SWITCH);
    start_record_tick(&record, 41005);
    StartInfo info = start_record_boot(&record);
    CHECK_EQUAL(HAL_START_STEP_CLOCK_SWITCH, info.last_step);
    CHECK(!info.last_step_finished);
    CHECK_EQUAL(41000u, info.last_step_ms);
    CHECK_EQUAL(HAL_START_STEP_NONE, (HalStartStep)record.step);
}

static void a_finished_step_is_marked_finished(void)
{
    StartRecord record = garbage();
    start_record_boot(&record);
    start_record_step_begin(&record, HAL_START_STEP_BACKLIGHT_ON);
    start_record_step_end(&record);
    StartInfo info = start_record_boot(&record);
    CHECK_EQUAL(HAL_START_STEP_BACKLIGHT_ON, info.last_step);
    CHECK(info.last_step_finished);
}

static void the_last_battery_reading_survives(void)
{
    StartRecord record = garbage();
    start_record_boot(&record);
    start_record_battery(&record, 3712);
    StartInfo info = start_record_boot(&record);
    CHECK_EQUAL(3712u, info.last_millivolts);
    CHECK_EQUAL(0u, record.millivolts);
}

static void a_cold_start_has_no_last_step(void)
{
    StartRecord record = garbage();
    record.step = HAL_START_STEP_CORE1_STOP;
    StartInfo info = start_record_boot(&record);
    CHECK_EQUAL(HAL_START_STEP_NONE, info.last_step);
    CHECK_EQUAL(0u, info.last_millivolts);
}

static void an_installed_update_survives_with_the_emmc_state_it_left(void)
{
    StartRecord record = garbage();
    start_record_boot(&record);
    start_record_update(&record, (HalUpdateRecord){.result = HAL_UPDATE_INSTALLED,
                                                   .detail = 100,
                                                   .emmc_replied = true,
                                                   .emmc_status = 0x00000900u});
    StartInfo info = start_record_boot(&record);
    CHECK_EQUAL(HAL_UPDATE_INSTALLED, info.last_update.result);
    CHECK_EQUAL(100, info.last_update.detail);
    CHECK(info.last_update.emmc_replied);
    CHECK_EQUAL(0x00000900u, info.last_update.emmc_status);
    CHECK_EQUAL(HAL_UPDATE_NONE, record.update_result);
    CHECK_EQUAL(0xFFFFFFFFu, record.update_emmc);
}

static void a_failed_update_without_an_emmc_reply_says_so(void)
{
    StartRecord record = garbage();
    start_record_boot(&record);
    start_record_update(&record, (HalUpdateRecord){.result = HAL_UPDATE_FAILED,
                                                   .detail = 37,
                                                   .emmc_replied = false,
                                                   .emmc_status = 0x12345678u});
    StartInfo info = start_record_boot(&record);
    CHECK_EQUAL(HAL_UPDATE_FAILED, info.last_update.result);
    CHECK_EQUAL(37, info.last_update.detail);
    CHECK(!info.last_update.emmc_replied);
}

static void a_run_without_an_update_reports_none_and_a_bad_result_reads_as_none(void)
{
    StartRecord record = garbage();
    start_record_boot(&record);
    StartInfo info = start_record_boot(&record);
    CHECK_EQUAL(HAL_UPDATE_NONE, info.last_update.result);
    record.update_result = 99;
    info = start_record_boot(&record);
    CHECK_EQUAL(HAL_UPDATE_NONE, info.last_update.result);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_record_without_its_magic_reads_as_a_cold_start),
        TEST_CASE(a_surviving_record_reads_as_a_warm_start_with_the_last_uptime),
        TEST_CASE(a_corrupted_count_reads_as_cold),
        TEST_CASE(a_fault_survives_into_the_next_start),
        TEST_CASE(a_stall_survives_into_the_next_start_with_its_pc_and_lr),
        TEST_CASE(a_stall_keeps_the_stack_pointer_the_words_above_the_frame_and_the_depth),
        TEST_CASE(a_run_without_a_stall_reports_none),
        TEST_CASE(a_clean_power_off_is_remembered),
        TEST_CASE(the_last_step_survives_with_its_start_time_and_whether_it_finished),
        TEST_CASE(a_finished_step_is_marked_finished),
        TEST_CASE(the_last_battery_reading_survives),
        TEST_CASE(a_cold_start_has_no_last_step),
        TEST_CASE(an_installed_update_survives_with_the_emmc_state_it_left),
        TEST_CASE(a_failed_update_without_an_emmc_reply_says_so),
        TEST_CASE(a_run_without_an_update_reports_none_and_a_bad_result_reads_as_none),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

#include "check.h"
#include "device/start_record.h"

static StartRecord garbage(void)
{
    return (StartRecord){.magic = 0x12345678u, .starts = 99, .check = 7, .uptime_ms = 5000};
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

static void a_clean_power_off_is_remembered(void)
{
    StartRecord record = garbage();
    start_record_boot(&record);
    start_record_power_off(&record);
    StartInfo info = start_record_boot(&record);
    CHECK(info.warm);
    CHECK(info.previous_power_off);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_record_without_its_magic_reads_as_a_cold_start),
        TEST_CASE(a_surviving_record_reads_as_a_warm_start_with_the_last_uptime),
        TEST_CASE(a_corrupted_count_reads_as_cold),
        TEST_CASE(a_fault_survives_into_the_next_start),
        TEST_CASE(a_clean_power_off_is_remembered),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

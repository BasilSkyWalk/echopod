#include <string.h>

#include "app/update_record.h"
#include "check.h"
#include "fake_hal.h"

static HalUpdateRecord installed(void)
{
    return (HalUpdateRecord){.result = HAL_UPDATE_INSTALLED,
                             .detail = 100,
                             .emmc_replied = true,
                             .emmc_status = 0x00000900u};
}

static bool same(HalUpdateRecord left, HalUpdateRecord right)
{
    return left.result == right.result && left.detail == right.detail &&
           left.emmc_replied == right.emmc_replied && left.emmc_status == right.emmc_status;
}

static void a_record_survives_encoding(void)
{
    uint8_t bytes[UPDATE_RECORD_BYTES];
    HalUpdateRecord decoded;
    update_record_encode(installed(), bytes);
    CHECK(memcmp(bytes, "EUPD", 4) == 0);
    CHECK(update_record_decode(bytes, &decoded));
    CHECK(same(installed(), decoded));
}

static void a_damaged_record_is_refused(void)
{
    uint8_t bytes[UPDATE_RECORD_BYTES];
    HalUpdateRecord decoded;
    update_record_encode(installed(), bytes);
    bytes[5] = 37;
    CHECK(!update_record_decode(bytes, &decoded));
    update_record_encode(installed(), bytes);
    bytes[4] = HAL_UPDATE_RESULT_COUNT;
    CHECK(!update_record_decode(bytes, &decoded));
    memset(bytes, 0, sizeof bytes);
    CHECK(!update_record_decode(bytes, &decoded));
}

static void a_saved_record_reads_back_from_the_card(void)
{
    HalUpdateRecord read;
    fake_hal_reset();
    fake_hal_add_file(UPDATE_RECORD_PATH, NULL, 4096);
    CHECK(!update_record_read(&read));
    HalUpdateRecord failed = {
        .result = HAL_UPDATE_FAILED, .detail = 35, .emmc_replied = false, .emmc_status = 0};
    CHECK(update_record_save(failed));
    CHECK(update_record_read(&read));
    CHECK(same(failed, read));
}

static void a_card_without_the_file_has_no_record(void)
{
    HalUpdateRecord read;
    fake_hal_reset();
    CHECK(!update_record_read(&read));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_record_survives_encoding),
        TEST_CASE(a_damaged_record_is_refused),
        TEST_CASE(a_saved_record_reads_back_from_the_card),
        TEST_CASE(a_card_without_the_file_has_no_record),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

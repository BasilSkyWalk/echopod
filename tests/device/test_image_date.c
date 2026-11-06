#include "check.h"
#include "core/wall_clock.h"
#include "device/image_date.h"

static void the_image_header_date_gives_its_day(void)
{
    CHECK_EQUAL(wall_clock_days_from_civil(2026, 11, 6), image_date_day(0x11062026u));
    CHECK_EQUAL(wall_clock_days_from_civil(2025, 1, 31), image_date_day(0x01312025u));
}

static void a_date_that_is_not_a_date_gives_no_day(void)
{
    CHECK_EQUAL(0, image_date_day(0));
    CHECK_EQUAL(0, image_date_day(0xFFFFFFFFu));
    CHECK_EQUAL(0, image_date_day(0x13012026u));
    CHECK_EQUAL(0, image_date_day(0x02302026u));
    CHECK_EQUAL(0, image_date_day(0x1A062026u));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_image_header_date_gives_its_day),
        TEST_CASE(a_date_that_is_not_a_date_gives_no_day),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

#include "check.h"
#include "device/vbus_debounce.h"

static VbusDebounce started(void)
{
    VbusDebounce vbus;
    vbus_debounce_start(&vbus);
    return vbus;
}

static void a_steady_cable_stays_present(void)
{
    VbusDebounce vbus = started();
    for (uint32_t now = 0; now < 10000; now += 7) {
        CHECK(vbus_debounce_present(&vbus, true, now));
    }
    CHECK_EQUAL(0, vbus.dips);
}

static void one_missing_reading_is_ignored_and_counted(void)
{
    VbusDebounce vbus = started();
    CHECK(vbus_debounce_present(&vbus, true, 0));
    CHECK(vbus_debounce_present(&vbus, false, 1));
    CHECK(vbus_debounce_present(&vbus, true, 2));
    CHECK_EQUAL(1, vbus.dips);
}

static void a_dip_shorter_than_the_debounce_keeps_the_session(void)
{
    VbusDebounce vbus = started();
    for (uint32_t now = 1000; now < 1000 + VBUS_DEBOUNCE_MS; now++) {
        CHECK(vbus_debounce_present(&vbus, false, now));
    }
    CHECK(vbus_debounce_present(&vbus, true, 1000 + VBUS_DEBOUNCE_MS));
    CHECK_EQUAL(1, vbus.dips);
}

static void a_cable_gone_for_the_debounce_time_ends_the_session(void)
{
    VbusDebounce vbus = started();
    CHECK(vbus_debounce_present(&vbus, false, 500));
    CHECK(vbus_debounce_present(&vbus, false, 550));
    CHECK(!vbus_debounce_present(&vbus, false, 500 + VBUS_DEBOUNCE_MS));
    CHECK_EQUAL(0, vbus.dips);
}

static void a_long_gap_between_two_readings_is_not_enough(void)
{
    VbusDebounce vbus = started();
    CHECK(vbus_debounce_present(&vbus, false, 0));
    CHECK(vbus_debounce_present(&vbus, false, 3000));
    CHECK(!vbus_debounce_present(&vbus, false, 3001));
}

static void once_gone_it_stays_gone(void)
{
    VbusDebounce vbus = started();
    for (uint32_t now = 0; now <= VBUS_DEBOUNCE_MS; now += 10) {
        vbus_debounce_present(&vbus, false, now);
    }
    CHECK(!vbus_debounce_present(&vbus, true, VBUS_DEBOUNCE_MS + 1));
}

static void the_debounce_survives_the_clock_wrapping(void)
{
    VbusDebounce vbus = started();
    CHECK(vbus_debounce_present(&vbus, false, UINT32_MAX - 10));
    CHECK(vbus_debounce_present(&vbus, false, UINT32_MAX));
    CHECK(vbus_debounce_present(&vbus, false, 20));
    CHECK(!vbus_debounce_present(&vbus, false, VBUS_DEBOUNCE_MS));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_steady_cable_stays_present),
        TEST_CASE(one_missing_reading_is_ignored_and_counted),
        TEST_CASE(a_dip_shorter_than_the_debounce_keeps_the_session),
        TEST_CASE(a_cable_gone_for_the_debounce_time_ends_the_session),
        TEST_CASE(a_long_gap_between_two_readings_is_not_enough),
        TEST_CASE(once_gone_it_stays_gone),
        TEST_CASE(the_debounce_survives_the_clock_wrapping),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

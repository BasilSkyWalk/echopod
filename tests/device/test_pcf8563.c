#include "check.h"
#include "device/pcf8563.h"

static const uint8_t RUNNING[PCF8563_REGISTER_COUNT] = {0x00, 0x00, 0x22, 0x37, 0x04,
                                                        0x07, 0x03, 0x05, 0x25};

static void bcd_decodes_and_encodes_two_digits(void)
{
    CHECK_EQUAL(0, bcd_decode(0x00));
    CHECK_EQUAL(59, bcd_decode(0x59));
    CHECK_EQUAL(99, bcd_decode(0x99));
    CHECK_EQUAL(0x00, bcd_encode(0));
    CHECK_EQUAL(0x59, bcd_encode(59));
    CHECK_EQUAL(0x99, bcd_encode(99));
    for (unsigned value = 0; value < 100; value++) {
        CHECK_EQUAL(value, bcd_decode(bcd_encode((uint8_t)value)));
    }
}

static void the_read_frame_addresses_the_chip_at_its_first_register(void)
{
    CHECK_EQUAL(0xA2, PCF8563_WRITE_ADDRESS);
    CHECK_EQUAL(0x00, PCF8563_FIRST_REGISTER);
    CHECK_EQUAL(9, PCF8563_REGISTER_COUNT);
}

static void civil_dates_follow_the_calendar(void)
{
    CHECK(civil_date_valid(2026, 1, 31));
    CHECK(!civil_date_valid(2026, 4, 31));
    CHECK(civil_date_valid(2024, 2, 29));
    CHECK(!civil_date_valid(2025, 2, 29));
    CHECK(civil_date_valid(2000, 2, 29));
    CHECK(!civil_date_valid(2100, 2, 29));
    CHECK(!civil_date_valid(2026, 0, 1));
    CHECK(!civil_date_valid(2026, 13, 1));
    CHECK(!civil_date_valid(2026, 6, 0));
}

static void a_running_clock_decodes_as_a_valid_local_time(void)
{
    HalTime time;
    pcf8563_decode(RUNNING, &time);
    CHECK(time.present);
    CHECK(time.valid);
    CHECK(!time.low_voltage);
    CHECK(!time.stopped);
    CHECK(!time.century);
    CHECK_EQUAL(2025, time.year);
    CHECK_EQUAL(5, time.month);
    CHECK_EQUAL(7, time.day);
    CHECK_EQUAL(4, time.hour);
    CHECK_EQUAL(37, time.minute);
    CHECK_EQUAL(22, time.second);
    CHECK_EQUAL(3, time.weekday);
}

static void the_low_voltage_flag_makes_the_time_untrusted_but_still_decoded(void)
{
    uint8_t registers[PCF8563_REGISTER_COUNT];
    for (int index = 0; index < PCF8563_REGISTER_COUNT; index++) {
        registers[index] = RUNNING[index];
    }
    registers[2] = 0x80 | 0x22;
    HalTime time;
    pcf8563_decode(registers, &time);
    CHECK(time.present);
    CHECK(!time.valid);
    CHECK(time.low_voltage);
    CHECK_EQUAL(22, time.second);
    CHECK_EQUAL(2025, time.year);
}

static void a_stopped_clock_is_not_valid(void)
{
    uint8_t registers[PCF8563_REGISTER_COUNT];
    for (int index = 0; index < PCF8563_REGISTER_COUNT; index++) {
        registers[index] = RUNNING[index];
    }
    registers[0] = 0x20;
    HalTime time;
    pcf8563_decode(registers, &time);
    CHECK(!time.valid);
    CHECK(time.stopped);
}

static void unused_register_bits_are_ignored(void)
{
    static const uint8_t NOISY[PCF8563_REGISTER_COUNT] = {0x00, 0x00, 0x22, 0xB7, 0xC4,
                                                          0xC7, 0xF3, 0x65, 0x25};
    HalTime time;
    pcf8563_decode(NOISY, &time);
    CHECK(time.valid);
    CHECK(time.minute == 37 && time.hour == 4 && time.day == 7 && time.weekday == 3);
    CHECK_EQUAL(5, time.month);
    CHECK(!time.century);
}

static void the_century_bit_is_reported_but_the_year_stays_in_this_century(void)
{
    uint8_t registers[PCF8563_REGISTER_COUNT];
    for (int index = 0; index < PCF8563_REGISTER_COUNT; index++) {
        registers[index] = RUNNING[index];
    }
    registers[7] = 0x85;
    HalTime time;
    pcf8563_decode(registers, &time);
    CHECK(time.valid);
    CHECK(time.century);
    CHECK_EQUAL(2025, time.year);
}

static void out_of_range_fields_are_not_valid(void)
{
    static const uint8_t BAD[][PCF8563_REGISTER_COUNT] = {
        {0x00, 0x00, 0x60, 0x37, 0x04, 0x07, 0x03, 0x05, 0x25},
        {0x00, 0x00, 0x22, 0x60, 0x04, 0x07, 0x03, 0x05, 0x25},
        {0x00, 0x00, 0x22, 0x37, 0x24, 0x07, 0x03, 0x05, 0x25},
        {0x00, 0x00, 0x22, 0x37, 0x04, 0x32, 0x03, 0x05, 0x25},
        {0x00, 0x00, 0x22, 0x37, 0x04, 0x07, 0x07, 0x05, 0x25},
        {0x00, 0x00, 0x22, 0x37, 0x04, 0x07, 0x03, 0x13, 0x25},
        {0x00, 0x00, 0x22, 0x37, 0x04, 0x07, 0x03, 0x00, 0x25},
        {0x00, 0x00, 0x22, 0x37, 0x04, 0x00, 0x03, 0x05, 0x25},
        {0x00, 0x00, 0x22, 0x37, 0x04, 0x29, 0x03, 0x02, 0x25},
        {0x00, 0x00, 0x2A, 0x37, 0x04, 0x07, 0x03, 0x05, 0x25},
        {0x00, 0x00, 0x22, 0x37, 0x04, 0x07, 0x03, 0x05, 0xA5},
    };
    for (size_t index = 0; index < sizeof BAD / sizeof BAD[0]; index++) {
        HalTime time;
        pcf8563_decode(BAD[index], &time);
        CHECK(time.present);
        CHECK(!time.valid);
    }
}

static void a_leap_day_is_valid_in_a_leap_year(void)
{
    static const uint8_t LEAP[PCF8563_REGISTER_COUNT] = {0x00, 0x00, 0x00, 0x00, 0x00,
                                                         0x29, 0x04, 0x02, 0x24};
    HalTime time;
    pcf8563_decode(LEAP, &time);
    CHECK(time.valid);
    CHECK_EQUAL(29, time.day);
    CHECK_EQUAL(2024, time.year);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(bcd_decodes_and_encodes_two_digits),
        TEST_CASE(the_read_frame_addresses_the_chip_at_its_first_register),
        TEST_CASE(civil_dates_follow_the_calendar),
        TEST_CASE(a_running_clock_decodes_as_a_valid_local_time),
        TEST_CASE(the_low_voltage_flag_makes_the_time_untrusted_but_still_decoded),
        TEST_CASE(a_stopped_clock_is_not_valid),
        TEST_CASE(unused_register_bits_are_ignored),
        TEST_CASE(the_century_bit_is_reported_but_the_year_stays_in_this_century),
        TEST_CASE(out_of_range_fields_are_not_valid),
        TEST_CASE(a_leap_day_is_valid_in_a_leap_year),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

#include "check.h"
#include "device/jack_sense.h"

static void the_pins_name_the_plugged_jack(void)
{
    CHECK_EQUAL(JACK_NONE, jack_kind_of(false, true));
    CHECK_EQUAL(JACK_SINGLE_ENDED, jack_kind_of(true, true));
    CHECK_EQUAL(JACK_BALANCED, jack_kind_of(false, false));
    CHECK_EQUAL(JACK_BALANCED, jack_kind_of(true, false));
}

static void the_first_reading_is_taken_at_once(void)
{
    JackSense sense = {0};
    CHECK(jack_sense_sample(&sense, JACK_SINGLE_ENDED));
    CHECK_EQUAL(JACK_SINGLE_ENDED, sense.stable);
}

static void a_change_needs_seven_matching_readings(void)
{
    JackSense sense = {0};
    jack_sense_sample(&sense, JACK_SINGLE_ENDED);
    for (int reading = 1; reading < JACK_SENSE_SAMPLES; reading++) {
        CHECK(!jack_sense_sample(&sense, JACK_NONE));
    }
    CHECK_EQUAL(JACK_SINGLE_ENDED, sense.stable);
    CHECK(jack_sense_sample(&sense, JACK_NONE));
    CHECK_EQUAL(JACK_NONE, sense.stable);
}

static void a_bouncing_contact_starts_the_count_again(void)
{
    JackSense sense = {0};
    jack_sense_sample(&sense, JACK_NONE);
    for (int reading = 1; reading < JACK_SENSE_SAMPLES; reading++) {
        jack_sense_sample(&sense, JACK_BALANCED);
    }
    CHECK(!jack_sense_sample(&sense, JACK_NONE));
    for (int reading = 1; reading < JACK_SENSE_SAMPLES; reading++) {
        CHECK(!jack_sense_sample(&sense, JACK_BALANCED));
    }
    CHECK_EQUAL(JACK_NONE, sense.stable);
    CHECK(jack_sense_sample(&sense, JACK_BALANCED));
    CHECK_EQUAL(JACK_BALANCED, sense.stable);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_pins_name_the_plugged_jack),
        TEST_CASE(the_first_reading_is_taken_at_once),
        TEST_CASE(a_change_needs_seven_matching_readings),
        TEST_CASE(a_bouncing_contact_starts_the_count_again),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

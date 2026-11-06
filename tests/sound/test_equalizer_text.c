#include "check.h"
#include "sound/equalizer_text.h"

#include <string.h>

static bool parse(const char *text, EqualizerCurve *curve, EqualizerTextError *error)
{
    return equalizer_text_parse(text, (uint32_t)strlen(text), curve, error);
}

static void the_example_preset_is_understood(void)
{
    static const char TEXT[] = "Preamp: -1.3175 dB\n"
                               "Filter 1: ON PK Fc 22 Hz Gain -0.6 dB Q 4.8\n"
                               "Filter 2: ON PK Fc 34 Hz Gain -1.7 dB Q 0.1\n"
                               "Filter 3: ON PK Fc 430 Hz Gain 1.9 dB Q 0.9\n"
                               "Filter 4: ON PK Fc 1700 Hz Gain -1.6 dB Q 1.2\n"
                               "Filter 5: ON PK Fc 3000 Hz Gain 1.8 dB Q 3.3\n";
    EqualizerCurve curve;
    EqualizerTextError error;
    CHECK(parse(TEXT, &curve, &error));
    CHECK_EQUAL(1, curve.preamp_set);
    CHECK_EQUAL(-13, curve.preamp_tenths);
    CHECK_EQUAL(5, curve.band_count);
    CHECK_EQUAL(22, curve.bands[0].frequency_hz);
    CHECK_EQUAL(-6, curve.bands[0].gain_tenths);
    CHECK_EQUAL(48, curve.bands[0].q_tenths);
    CHECK_EQUAL(34, curve.bands[1].frequency_hz);
    CHECK_EQUAL(-17, curve.bands[1].gain_tenths);
    CHECK_EQUAL(1, curve.bands[1].q_tenths);
    CHECK_EQUAL(3000, curve.bands[4].frequency_hz);
    CHECK_EQUAL(18, curve.bands[4].gain_tenths);
    CHECK_EQUAL(33, curve.bands[4].q_tenths);
}

static void crlf_bom_comments_and_off_filters_are_accepted(void)
{
    static const char TEXT[] = "\xEF\xBB\xBF# comment\r\n"
                               "\r\n"
                               "  Preamp : -2 dB  # trailing\r\n"
                               "Filter 1: OFF PK Fc 20 Hz Gain 12 dB Q 9\r\n"
                               "Filter 2: ON PK Fc 1000 Hz Gain -1.5 dB Q 0.7\r\n";
    EqualizerCurve curve;
    EqualizerTextError error;
    CHECK(parse(TEXT, &curve, &error));
    CHECK_EQUAL(-20, curve.preamp_tenths);
    CHECK_EQUAL(1, curve.band_count);
    CHECK_EQUAL(1000, curve.bands[0].frequency_hz);
    CHECK_EQUAL(-15, curve.bands[0].gain_tenths);
    CHECK_EQUAL(7, curve.bands[0].q_tenths);
}

static void a_preamp_only_preset_is_valid(void)
{
    static const char TEXT[] = "Preamp: -5.5 dB\n";
    EqualizerCurve curve;
    EqualizerTextError error;
    CHECK(parse(TEXT, &curve, &error));
    CHECK_EQUAL(0, curve.band_count);
    CHECK_EQUAL(-55, curve.preamp_tenths);
}

static void unsupported_filter_types_are_rejected(void)
{
    static const char TEXT[] = "Filter 1: ON LS Fc 100 Hz Gain 3 dB Q 0.7\n";
    EqualizerCurve curve;
    EqualizerTextError error;
    CHECK(!parse(TEXT, &curve, &error));
    CHECK_EQUAL(EQUALIZER_TEXT_UNSUPPORTED_FILTER, error.problem);
    CHECK_EQUAL(1, error.line);
}

static void duplicate_fields_and_filters_are_rejected(void)
{
    EqualizerCurve curve;
    EqualizerTextError error;
    CHECK(!parse("Filter 1: ON PK Fc 100 Hz Fc 200 Hz Gain 1 dB Q 1\n", &curve, &error));
    CHECK_EQUAL(EQUALIZER_TEXT_DUPLICATE_FIELD, error.problem);
    CHECK(!parse("Filter 1: ON PK Fc 100 Hz Gain 1 dB Q 1\n"
                 "Filter 1: ON PK Fc 200 Hz Gain 1 dB Q 1\n",
                 &curve, &error));
    CHECK_EQUAL(EQUALIZER_TEXT_DUPLICATE_FILTER, error.problem);
    CHECK_EQUAL(2, error.line);
    CHECK(!parse("Preamp: -1 dB\nPreamp: -2 dB\n", &curve, &error));
    CHECK_EQUAL(EQUALIZER_TEXT_DUPLICATE_PREAMP, error.problem);
}

static void malformed_and_out_of_range_values_are_rejected(void)
{
    EqualizerCurve curve;
    EqualizerTextError error;
    CHECK(!parse("Filter 1: ON PK Fc 100 Hz Gain 1 dB\n", &curve, &error));
    CHECK_EQUAL(EQUALIZER_TEXT_SYNTAX, error.problem);
    CHECK(!parse("Filter 1: ON PK Fc 100 Hz Gain 1 dB Q 0\n", &curve, &error));
    CHECK_EQUAL(EQUALIZER_TEXT_Q, error.problem);
    CHECK(!parse("Filter 1: ON PK Fc 100 Hz Gain 1 dB Q 10.1\n", &curve, &error));
    CHECK_EQUAL(EQUALIZER_TEXT_Q, error.problem);
    CHECK(parse("Filter 1: ON PK Fc 100 Hz Gain 1 dB Q 10\n", &curve, &error));
    CHECK(!parse("Filter 1: ON PK Fc 100 Hz Gain 13 dB Q 1\n", &curve, &error));
    CHECK_EQUAL(EQUALIZER_TEXT_GAIN, error.problem);
    CHECK(!parse("Filter 1: ON PK Fc 43200 Hz Gain 1 dB Q 1\n", &curve, &error));
    CHECK_EQUAL(EQUALIZER_TEXT_FREQUENCY, error.problem);
    CHECK(!parse("Filter 1: ON PK Fc 100 Hz Gain 1 dB Q 1 dB\n", &curve, &error));
    CHECK_EQUAL(EQUALIZER_TEXT_SYNTAX, error.problem);
    CHECK(!parse("GraphicEQ: 20 0\n", &curve, &error));
    CHECK_EQUAL(EQUALIZER_TEXT_UNSUPPORTED_DIRECTIVE, error.problem);
    CHECK(!parse("", &curve, &error));
    CHECK_EQUAL(EQUALIZER_TEXT_EMPTY, error.problem);
}

static void too_many_filters_and_long_lines_are_rejected(void)
{
    static const char FILTERS[] = "Filter 1: ON PK Fc 100 Hz Gain 1 dB Q 1\n"
                                  "Filter 2: ON PK Fc 200 Hz Gain 1 dB Q 1\n"
                                  "Filter 3: ON PK Fc 300 Hz Gain 1 dB Q 1\n"
                                  "Filter 4: ON PK Fc 400 Hz Gain 1 dB Q 1\n"
                                  "Filter 5: ON PK Fc 500 Hz Gain 1 dB Q 1\n"
                                  "Filter 6: ON PK Fc 600 Hz Gain 1 dB Q 1\n"
                                  "Filter 7: ON PK Fc 700 Hz Gain 1 dB Q 1\n"
                                  "Filter 8: ON PK Fc 800 Hz Gain 1 dB Q 1\n"
                                  "Filter 9: ON PK Fc 900 Hz Gain 1 dB Q 1\n";
    static const char LONG[] = "Filter 1: ON PK Fc 100 Hz Gain 1 dB Q 1 "
                               "################################################################"
                               "################################################################";
    EqualizerCurve curve;
    EqualizerTextError error;
    CHECK(!parse(FILTERS, &curve, &error));
    CHECK_EQUAL(EQUALIZER_TEXT_TOO_MANY_FILTERS, error.problem);
    CHECK_EQUAL(9, error.line);
    CHECK(!parse(LONG, &curve, &error));
    CHECK_EQUAL(EQUALIZER_TEXT_LINE_TOO_LONG, error.problem);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_example_preset_is_understood),
        TEST_CASE(crlf_bom_comments_and_off_filters_are_accepted),
        TEST_CASE(a_preamp_only_preset_is_valid),
        TEST_CASE(unsupported_filter_types_are_rejected),
        TEST_CASE(duplicate_fields_and_filters_are_rejected),
        TEST_CASE(malformed_and_out_of_range_values_are_rejected),
        TEST_CASE(too_many_filters_and_long_lines_are_rejected),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

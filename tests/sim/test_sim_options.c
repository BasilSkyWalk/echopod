#include <stddef.h>

#include "check.h"
#include "sim_options.h"

enum {
    MAX_ARGUMENTS = 8
};

static bool parse(char **given, int count, SimOptions *options)
{
    char *arguments[MAX_ARGUMENTS] = {"echo_sim"};
    for (int index = 0; index < count; index++) {
        arguments[index + 1] = given[index];
    }
    return sim_options_parse(count + 1, arguments, options);
}

static void no_arguments_give_a_windowed_run_at_default_scale(void)
{
    SimOptions options;
    CHECK(parse(NULL, 0, &options));
    CHECK_EQUAL(SIM_DEFAULT_SCALE, options.scale);
    CHECK_EQUAL(0, options.frame_limit);
    CHECK(!options.headless);
    CHECK(options.screenshot_path == NULL);
    CHECK(options.key_script == NULL);
    CHECK(options.card_directory == NULL);
    CHECK(options.system_directory == NULL);
}

static void storage_directories_are_read(void)
{
    char *given[] = {"--card", "/music", "--system-dir", "/tmp/echo"};
    SimOptions options;
    CHECK(parse(given, 4, &options));
    CHECK(options.card_directory == given[1]);
    CHECK(options.system_directory == given[3]);
}

static void a_key_script_is_read(void)
{
    char *given[] = {"--keys", "down,play"};
    SimOptions options;
    CHECK(parse(given, 2, &options));
    CHECK(options.key_script == given[1]);
}

static void every_option_is_read(void)
{
    char *given[] = {"--scale", "2", "--frames", "10", "--headless", "--screenshot", "a.bmp"};
    SimOptions options;
    CHECK(parse(given, 7, &options));
    CHECK_EQUAL(2, options.scale);
    CHECK_EQUAL(10, options.frame_limit);
    CHECK(options.headless);
    CHECK(options.screenshot_path == given[6]);
}

static void an_unknown_option_is_rejected(void)
{
    char *given[] = {"--fast"};
    SimOptions options;
    CHECK(!parse(given, 1, &options));
}

static void an_option_missing_its_value_is_rejected(void)
{
    char *given[] = {"--frames"};
    SimOptions options;
    CHECK(!parse(given, 1, &options));
}

static void non_numeric_values_are_rejected(void)
{
    char *given[] = {"--frames", "ten"};
    SimOptions options;
    CHECK(!parse(given, 2, &options));
}

static void a_scale_outside_the_supported_range_is_rejected(void)
{
    char *too_small[] = {"--scale", "0"};
    char *too_large[] = {"--scale", "9"};
    SimOptions options;
    CHECK(!parse(too_small, 2, &options));
    CHECK(!parse(too_large, 2, &options));
}

static void a_headless_run_needs_a_frame_limit(void)
{
    char *given[] = {"--headless"};
    SimOptions options;
    CHECK(!parse(given, 1, &options));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(no_arguments_give_a_windowed_run_at_default_scale),
        TEST_CASE(every_option_is_read),
        TEST_CASE(a_key_script_is_read),
        TEST_CASE(storage_directories_are_read),
        TEST_CASE(an_unknown_option_is_rejected),
        TEST_CASE(an_option_missing_its_value_is_rejected),
        TEST_CASE(non_numeric_values_are_rejected),
        TEST_CASE(a_scale_outside_the_supported_range_is_rejected),
        TEST_CASE(a_headless_run_needs_a_frame_limit),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

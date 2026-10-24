#include <string.h>

#include "check.h"
#include "core/string_builder.h"

enum {
    SMALL = 6,
    LARGE = 32
};

static void appended_text_is_terminated(void)
{
    char buffer[LARGE];
    StringBuilder builder = string_builder_make(buffer, sizeof buffer);
    string_builder_append(&builder, "Echo");
    string_builder_append_char(&builder, ' ');
    string_builder_append(&builder, "Mini");
    CHECK(strcmp("Echo Mini", buffer) == 0);
    CHECK_EQUAL(9, builder.length);
    CHECK(!builder.overflowed);
}

static void unsigned_values_are_written_in_decimal(void)
{
    char buffer[LARGE];
    StringBuilder builder = string_builder_make(buffer, sizeof buffer);
    string_builder_append_unsigned(&builder, 0);
    string_builder_append_char(&builder, ',');
    string_builder_append_unsigned(&builder, 4294967295u);
    CHECK(strcmp("0,4294967295", buffer) == 0);
}

static void overflow_keeps_the_longest_prefix_that_fits(void)
{
    char buffer[SMALL];
    StringBuilder builder = string_builder_make(buffer, sizeof buffer);
    string_builder_append(&builder, "Overflowing");
    CHECK(strcmp("Overf", buffer) == 0);
    CHECK(builder.overflowed);
}

static void a_number_that_does_not_fit_is_cut_like_text(void)
{
    char buffer[SMALL];
    StringBuilder builder = string_builder_make(buffer, sizeof buffer);
    string_builder_append(&builder, "ab");
    string_builder_append_unsigned(&builder, 123456);
    CHECK(strcmp("ab123", buffer) == 0);
    CHECK(builder.overflowed);
}

static void a_fresh_builder_holds_an_empty_string(void)
{
    char buffer[SMALL] = "junk";
    StringBuilder builder = string_builder_make(buffer, sizeof buffer);
    CHECK(strcmp("", buffer) == 0);
    CHECK_EQUAL(0, builder.length);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(appended_text_is_terminated),
        TEST_CASE(unsigned_values_are_written_in_decimal),
        TEST_CASE(overflow_keeps_the_longest_prefix_that_fits),
        TEST_CASE(a_number_that_does_not_fit_is_cut_like_text),
        TEST_CASE(a_fresh_builder_holds_an_empty_string),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

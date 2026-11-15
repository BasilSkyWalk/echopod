#include <stdio.h>
#include <string.h>

#include "app/conf_file.h"
#include "check.h"
#include "fake_hal.h"
#include "ui/toast.h"

static EchoConf conf;
static Toast toast;

static void load(const char *text)
{
    fake_hal_reset();
    toast = (Toast){0};
    if (text != NULL) {
        fake_hal_add_file("/echo.conf", (const uint8_t *)text, (uint32_t)strlen(text));
    }
    conf_file_load(&conf, &toast, 0);
}

static bool toast_says_error(void)
{
    return toast.visible && strcmp("Error parsing config file", toast.text) == 0;
}

static void without_a_file_the_default_cover_is_off(void)
{
    load(NULL);
    CHECK_EQUAL(0, conf.values[CONF_DEFAULT_COVER]);
    CHECK(!toast.visible);
}

static void the_default_cover_is_read(void)
{
    load("CONF_DEFAULT_COVER=1\n");
    CHECK_EQUAL(1, conf.values[CONF_DEFAULT_COVER]);
    CHECK(!toast.visible);
}

static void a_digit_inside_an_unknown_key_is_accepted(void)
{
    load("CONF_UNKNOWN2=1\nCONF_DEFAULT_COVER=1\n");
    CHECK_EQUAL(1, conf.values[CONF_DEFAULT_COVER]);
    CHECK(!toast.visible);
}

static void blank_lines_and_windows_line_endings_are_accepted(void)
{
    load("\r\nCONF_DEFAULT_COVER=1\r\n\r\n");
    CHECK_EQUAL(1, conf.values[CONF_DEFAULT_COVER]);
    CHECK(!toast.visible);
}

static void unknown_keys_are_skipped(void)
{
    load("CONF_UNKNOWN=7\nCONF_DEFAULT_COVER=1\n");
    CHECK_EQUAL(1, conf.values[CONF_DEFAULT_COVER]);
    CHECK(!toast.visible);
}

static void the_largest_value_is_accepted(void)
{
    load("CONF_DEFAULT_COVER=255\n");
    CHECK_EQUAL(255, conf.values[CONF_DEFAULT_COVER]);
    CHECK(!toast.visible);
}

static void a_value_above_255_resets_the_key_and_toasts(void)
{
    load("CONF_DEFAULT_COVER=256\n");
    CHECK_EQUAL(0, conf.values[CONF_DEFAULT_COVER]);
    CHECK(toast_says_error());
}

static void a_non_digit_value_resets_the_key_and_toasts(void)
{
    load("CONF_DEFAULT_COVER=on\n");
    CHECK_EQUAL(0, conf.values[CONF_DEFAULT_COVER]);
    CHECK(toast_says_error());
}

static void an_empty_value_resets_the_key_and_toasts(void)
{
    load("CONF_DEFAULT_COVER=\n");
    CHECK_EQUAL(0, conf.values[CONF_DEFAULT_COVER]);
    CHECK(toast_says_error());
}

static void a_line_without_an_equals_sign_resets_the_key_and_toasts(void)
{
    load("CONF_DEFAULT_COVER\n");
    CHECK_EQUAL(0, conf.values[CONF_DEFAULT_COVER]);
    CHECK(toast_says_error());
}

static void a_line_longer_than_48_characters_resets_the_key_and_toasts(void)
{
    load("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=1\n");
    CHECK_EQUAL(0, conf.values[CONF_DEFAULT_COVER]);
    CHECK(toast_says_error());
}

static void a_48_character_line_with_crlf_is_accepted(void)
{
    char text[80];
    uint32_t length = 0;
    for (int index = 0; index < 46; index++) {
        text[length++] = 'A';
    }
    length +=
        (uint32_t)snprintf(text + length, sizeof text - length, "=1\r\nCONF_DEFAULT_COVER=1\n");
    load(text);
    CHECK_EQUAL(1, conf.values[CONF_DEFAULT_COVER]);
    CHECK(!toast.visible);
}

static void a_file_over_640_bytes_is_ignored_silently(void)
{
    char text[642];
    memset(text, 'A', sizeof text - 1);
    text[sizeof text - 1] = '\0';
    load(text);
    CHECK_EQUAL(0, conf.values[CONF_DEFAULT_COVER]);
    CHECK(!toast.visible);
}

static void only_the_first_20_rows_are_read(void)
{
    char text[512];
    uint32_t length = 0;
    for (int row = 0; row < 20; row++) {
        length += (uint32_t)snprintf(text + length, sizeof text - length, "CONF_UNKNOWN=1\n");
    }
    length += (uint32_t)snprintf(text + length, sizeof text - length, "CONF_DEFAULT_COVER=1\n");
    load(text);
    CHECK_EQUAL(0, conf.values[CONF_DEFAULT_COVER]);
    CHECK(!toast.visible);
}

static void key_names_are_reported(void)
{
    CHECK(strcmp("CONF_DEFAULT_COVER", conf_key_name(CONF_DEFAULT_COVER)) == 0);
    CHECK(strcmp("", conf_key_name(CONF_COUNT)) == 0);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(without_a_file_the_default_cover_is_off),
        TEST_CASE(the_default_cover_is_read),
        TEST_CASE(a_digit_inside_an_unknown_key_is_accepted),
        TEST_CASE(blank_lines_and_windows_line_endings_are_accepted),
        TEST_CASE(unknown_keys_are_skipped),
        TEST_CASE(the_largest_value_is_accepted),
        TEST_CASE(a_value_above_255_resets_the_key_and_toasts),
        TEST_CASE(a_non_digit_value_resets_the_key_and_toasts),
        TEST_CASE(an_empty_value_resets_the_key_and_toasts),
        TEST_CASE(a_line_without_an_equals_sign_resets_the_key_and_toasts),
        TEST_CASE(a_line_longer_than_48_characters_resets_the_key_and_toasts),
        TEST_CASE(a_48_character_line_with_crlf_is_accepted),
        TEST_CASE(a_file_over_640_bytes_is_ignored_silently),
        TEST_CASE(only_the_first_20_rows_are_read),
        TEST_CASE(key_names_are_reported),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

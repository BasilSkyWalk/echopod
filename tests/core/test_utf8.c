#include "check.h"
#include "core/utf8.h"

static uint32_t decode_at(const char **cursor)
{
    return utf8_next(cursor);
}

static void ascii_bytes_decode_to_themselves(void)
{
    const char *cursor = "Az";
    CHECK_EQUAL('A', decode_at(&cursor));
    CHECK_EQUAL('z', decode_at(&cursor));
    CHECK_EQUAL(0, decode_at(&cursor));
}

static void multi_byte_sequences_decode_to_their_codepoints(void)
{
    const char *cursor = "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80";
    CHECK_EQUAL(0xE9, decode_at(&cursor));
    CHECK_EQUAL(0x20AC, decode_at(&cursor));
    CHECK_EQUAL(0x1F600, decode_at(&cursor));
    CHECK_EQUAL(0, decode_at(&cursor));
}

static void the_end_of_the_string_is_not_passed(void)
{
    const char *text = "";
    const char *cursor = text;
    CHECK_EQUAL(0, decode_at(&cursor));
    CHECK(cursor == text);
}

static void a_stray_continuation_byte_is_replaced_and_decoding_resumes(void)
{
    const char *cursor = "\x80"
                         "A";
    CHECK_EQUAL(UTF8_REPLACEMENT_CHARACTER, decode_at(&cursor));
    CHECK_EQUAL('A', decode_at(&cursor));
}

static void a_truncated_sequence_is_replaced(void)
{
    const char *cursor = "\xC3";
    CHECK_EQUAL(UTF8_REPLACEMENT_CHARACTER, decode_at(&cursor));
    CHECK_EQUAL(0, decode_at(&cursor));
}

static void an_overlong_encoding_is_rejected(void)
{
    const char *cursor = "\xC0\xAF"
                         "B";
    CHECK_EQUAL(UTF8_REPLACEMENT_CHARACTER, decode_at(&cursor));
    CHECK_EQUAL(UTF8_REPLACEMENT_CHARACTER, decode_at(&cursor));
    CHECK_EQUAL('B', decode_at(&cursor));
}

static void an_encoded_surrogate_is_rejected(void)
{
    const char *cursor = "\xED\xA0\x80";
    CHECK_EQUAL(UTF8_REPLACEMENT_CHARACTER, decode_at(&cursor));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(ascii_bytes_decode_to_themselves),
        TEST_CASE(multi_byte_sequences_decode_to_their_codepoints),
        TEST_CASE(the_end_of_the_string_is_not_passed),
        TEST_CASE(a_stray_continuation_byte_is_replaced_and_decoding_resumes),
        TEST_CASE(a_truncated_sequence_is_replaced),
        TEST_CASE(an_overlong_encoding_is_rejected),
        TEST_CASE(an_encoded_surrogate_is_rejected),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

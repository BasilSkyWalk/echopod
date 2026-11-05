#include "check.h"
#include "device/stack_mark.h"

enum {
    WORDS = 64
};

static void a_painted_stack_is_untouched_to_its_top(void)
{
    uint32_t stack[WORDS];
    stack_mark_paint(stack, WORDS);
    CHECK_EQUAL(WORDS, stack_mark_untouched_words(stack, WORDS));
}

static void the_deepest_word_written_sets_the_high_water_mark(void)
{
    uint32_t stack[WORDS];
    stack_mark_paint(stack, WORDS);
    stack[WORDS - 1] = 0;
    stack[WORDS - 10] = 0x12345678u;
    stack[WORDS - 6] = STACK_MARK_PATTERN;
    CHECK_EQUAL(WORDS - 10, stack_mark_untouched_words(stack, WORDS));
}

static void a_pattern_left_below_the_deepest_write_still_counts_as_untouched(void)
{
    uint32_t stack[WORDS];
    stack_mark_paint(stack, WORDS);
    stack[3] = STACK_MARK_PATTERN + 1;
    CHECK_EQUAL(3, stack_mark_untouched_words(stack, WORDS));
}

static void an_unpainted_stack_reads_as_fully_used(void)
{
    uint32_t stack[WORDS];
    for (uint32_t index = 0; index < WORDS; index++) {
        stack[index] = index;
    }
    CHECK_EQUAL(0, stack_mark_untouched_words(stack, WORDS));
    CHECK_EQUAL(0, stack_mark_untouched_words(stack, 0));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_painted_stack_is_untouched_to_its_top),
        TEST_CASE(the_deepest_word_written_sets_the_high_water_mark),
        TEST_CASE(a_pattern_left_below_the_deepest_write_still_counts_as_untouched),
        TEST_CASE(an_unpainted_stack_reads_as_fully_used),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

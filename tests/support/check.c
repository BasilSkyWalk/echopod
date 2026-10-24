#include "check.h"

#include <stdio.h>

static const char *current_test_name = "";
static int current_test_failures;

static void report_failure(const char *file, int line)
{
    current_test_failures++;
    fprintf(stderr, "  %s:%d: in %s\n", file, line, current_test_name);
}

void check_true(bool condition, const char *expression, const char *file, int line)
{
    if (condition) {
        return;
    }
    report_failure(file, line);
    fprintf(stderr, "    expected true: %s\n", expression);
}

void check_equal(long long expected, long long actual, const char *expression, const char *file,
                 int line)
{
    if (expected == actual) {
        return;
    }
    report_failure(file, line);
    fprintf(stderr, "    %s: expected %lld, got %lld\n", expression, expected, actual);
}

static bool run_case(const TestCase *test)
{
    current_test_name = test->name;
    current_test_failures = 0;
    test->run();
    printf("%s %s\n", current_test_failures == 0 ? "ok  " : "FAIL", test->name);
    return current_test_failures == 0;
}

int test_run_all(const TestCase *cases, size_t count)
{
    size_t passed = 0;
    for (size_t index = 0; index < count; index++) {
        if (run_case(&cases[index])) {
            passed++;
        }
    }
    printf("%zu/%zu passed\n", passed, count);
    return passed == count ? 0 : 1;
}

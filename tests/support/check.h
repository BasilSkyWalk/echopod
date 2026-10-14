#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef void (*TestFunction)(void);

typedef struct TestCase {
    const char *name;
    TestFunction run;
} TestCase;

#define TEST_CASE(function) {#function, function}
#define TEST_CASE_COUNT(cases) (sizeof(cases) / sizeof((cases)[0]))

#define CHECK(condition) check_true((condition), #condition, __FILE__, __LINE__)
#define CHECK_EQUAL(expected, actual)                                                              \
    check_equal((long long)(expected), (long long)(actual), #actual, __FILE__, __LINE__)

void check_true(bool condition, const char *expression, const char *file, int line);
void check_equal(long long expected, long long actual, const char *expression, const char *file,
                 int line);
int test_run_all(const TestCase *cases, size_t count);

#include <string.h>

#include "check.h"
#include "default_theme.h"
#include "screens/stopwatch_screen.h"

enum {
    START_MS = 1000
};

static StopwatchScreen stopwatch;

static Screen *open_stopwatch(void)
{
    Screen *screen = stopwatch_screen_init(&stopwatch, &DEFAULT_THEME);
    screen->enter(screen->context, START_MS);
    return screen;
}

static void the_stopwatch_starts_at_zero(void)
{
    Screen *screen = open_stopwatch();
    screen->update(screen->context, START_MS);
    CHECK(strcmp("0:00", stopwatch_screen_text(&stopwatch)) == 0);
}

static void the_stopwatch_counts_whole_seconds(void)
{
    Screen *screen = open_stopwatch();
    screen->update(screen->context, START_MS + 7999);
    CHECK(strcmp("0:07", stopwatch_screen_text(&stopwatch)) == 0);
    screen->update(screen->context, START_MS + 61000);
    CHECK(strcmp("1:01", stopwatch_screen_text(&stopwatch)) == 0);
}

static void reopening_restarts_the_count(void)
{
    Screen *screen = open_stopwatch();
    screen->update(screen->context, START_MS + 5000);
    screen->enter(screen->context, START_MS + 9000);
    screen->update(screen->context, START_MS + 9000);
    CHECK(strcmp("0:00", stopwatch_screen_text(&stopwatch)) == 0);
}

static void the_stopwatch_stays_open_on_wake(void)
{
    CHECK(open_stopwatch()->stays_on_wake);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_stopwatch_starts_at_zero),
        TEST_CASE(the_stopwatch_counts_whole_seconds),
        TEST_CASE(reopening_restarts_the_count),
        TEST_CASE(the_stopwatch_stays_open_on_wake),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

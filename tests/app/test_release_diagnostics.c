#include <string.h>

#include "app_screens.h"
#include "check.h"
#include "default_theme.h"
#include "fake_hal.h"

static AppScreens screens;
static AppOptions options;

static const MenuConfig *built(void)
{
    memset(&screens, 0, sizeof screens);
    diagnostics_menu_build(&screens, &options, &DEFAULT_THEME);
    return &screens.diagnostics.config;
}

static bool has_row(const MenuConfig *menu, const char *label)
{
    for (int index = 0; index < menu->item_count; index++) {
        if (strcmp(menu->items[index].label, label) == 0) {
            return true;
        }
    }
    return false;
}

static void the_release_menu_keeps_only_the_safe_rows(void)
{
    static const char *const SAFE[] = {"Key test",   "Firmware",     "Battery",  "Last start",
                                       "Last step",  "Last USB",     "Jack 3.5", "Jack 4.4",
                                       "USB errors", "Last battery", "Panel",    "Clock"};
    const MenuConfig *menu = built();
    CHECK_EQUAL(12, menu->item_count);
    for (size_t index = 0; index < sizeof SAFE / sizeof SAFE[0]; index++) {
        CHECK(has_row(menu, SAFE[index]));
    }
}

static void the_release_menu_has_no_tests_that_act_on_the_hardware(void)
{
    static const char *const DEVELOPER[] = {
        "Test tone", "DMA tone",  "Tone 192k",     "Tone 176k", "Stall test", "Watchdog test",
        "Core 1",    "CPU clock", "Core 1 decode", "LPC accel", "High RAM",   "Bluetooth"};
    const MenuConfig *menu = built();
    for (size_t index = 0; index < sizeof DEVELOPER / sizeof DEVELOPER[0]; index++) {
        CHECK(!has_row(menu, DEVELOPER[index]));
    }
}

static void every_value_row_reads_without_a_selection(void)
{
    const MenuConfig *menu = built();
    for (int index = 0; index < menu->item_count; index++) {
        const MenuItem *item = &menu->items[index];
        if (!item->opens) {
            CHECK(item->select == NULL || strcmp(item->label, "Clock") == 0);
            CHECK(item->value != NULL);
            CHECK(item->value(item->context) != NULL);
        }
    }
}

static const MenuItem *clock_row(const MenuConfig *menu)
{
    for (int index = 0; index < menu->item_count; index++) {
        if (strcmp(menu->items[index].label, "Clock") == 0) {
            return &menu->items[index];
        }
    }
    return NULL;
}

static void the_clock_row_only_reads_the_clock_and_reads_again_when_pressed(void)
{
    fake_hal_reset();
    const MenuConfig *menu = built();
    const MenuItem *clock = clock_row(menu);
    CHECK(clock != NULL);
    CHECK(strcmp("No reply", clock->value(clock->context)) == 0);
    int reads = fake_hal_time_read_count();
    fake_hal_set_time((HalTime){.present = true,
                                .valid = true,
                                .year = 2026,
                                .month = 11,
                                .day = 5,
                                .hour = 9,
                                .minute = 8,
                                .second = 7});
    CHECK(strcmp("No reply", clock->value(clock->context)) == 0);
    CHECK_EQUAL(SCREEN_ACTION_NONE, clock->select(clock->context).type);
    CHECK(strcmp("2026-11-05 09:08:07", clock->value(clock->context)) == 0);
    CHECK_EQUAL(reads + 1, fake_hal_time_read_count());
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_release_menu_keeps_only_the_safe_rows),
        TEST_CASE(the_release_menu_has_no_tests_that_act_on_the_hardware),
        TEST_CASE(every_value_row_reads_without_a_selection),
        TEST_CASE(the_clock_row_only_reads_the_clock_and_reads_again_when_pressed),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

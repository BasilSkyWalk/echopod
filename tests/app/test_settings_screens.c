#include <string.h>

#include "check.h"
#include "default_theme.h"
#include "fake_hal.h"
#include "settings_screens.h"

enum {
    TOP_GROUPS = 5
};

static SettingsScreens screens;

static const char *const TOP[SETTINGS_TOP_COUNT] = {
    "Shuffle", "Repeat",  "Bluetooth", "Equalizer", "Sleep timer",
    "Sound",   "Display", "Library",   "Buttons",   "System"};

static const char *const SOUND[] = {"Gain", "Output", "Filter", "Balance", "Bluetooth level"};
static const char *const DISPLAY[] = {
    "Theme",       "Accent color", "Highlight",       "Brightness",          "Screen timeout",
    "Orientation", "Animations",   "Battery percent", "Portrait status bar", "Clock"};
static const char *const LIBRARY[] = {"Music folders",       "Rescan library",   "Start screen",
                                      "Cover Flow",          "Cover Flow order", "Artist albums",
                                      "Folders & playlists", "Lyrics",           "Listen history"};
static const char *const BUTTONS[] = {"Volume buttons", "Swap volume buttons", "Menu button",
                                      "Power on"};
static const char *const SYSTEM[] = {"Language", "Date & time", "Auto off", "USB mode",
                                     "Ask when connected"};

typedef struct GroupRows {
    const char *title;
    const char *const *labels;
    int count;
} GroupRows;

static const GroupRows GROUPS[SETTINGS_GROUP_COUNT] = {
    {"Sound", SOUND, (int)(sizeof SOUND / sizeof SOUND[0])},
    {"Display", DISPLAY, (int)(sizeof DISPLAY / sizeof DISPLAY[0])},
    {"Library", LIBRARY, (int)(sizeof LIBRARY / sizeof LIBRARY[0])},
    {"Buttons", BUTTONS, (int)(sizeof BUTTONS / sizeof BUTTONS[0])},
    {"System", SYSTEM, (int)(sizeof SYSTEM / sizeof SYSTEM[0])},
};

static Screen *start(void)
{
    fake_hal_reset();
    return settings_screens_init(&screens, (SettingsContext){0}, &DEFAULT_THEME);
}

static Screen *open_group(SettingsGroup group)
{
    const MenuItem *item = &screens.items[TOP_GROUPS + group];
    ScreenAction action = item->select(item->context);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, action.type);
    return action.target;
}

static void the_top_level_lists_the_quick_settings_then_the_groups(void)
{
    Screen *top = start();
    CHECK_EQUAL(0, strcmp("Settings", top->title));
    CHECK_EQUAL(SETTINGS_TOP_COUNT, screens.menu.config.item_count);
    for (int index = 0; index < SETTINGS_TOP_COUNT; index++) {
        CHECK_EQUAL(0, strcmp(TOP[index], screens.items[index].label));
        CHECK(screens.items[index].opens);
    }
    for (int group = 0; group < SETTINGS_GROUP_COUNT; group++) {
        CHECK(screens.items[TOP_GROUPS + group].value == NULL);
    }
}

static void each_group_lists_its_rows_in_order(void)
{
    start();
    for (int group = 0; group < SETTINGS_GROUP_COUNT; group++) {
        Screen *screen = open_group((SettingsGroup)group);
        CHECK(screen == &screens.group.screen);
        CHECK_EQUAL(0, strcmp(GROUPS[group].title, screen->title));
        CHECK_EQUAL(GROUPS[group].count, screens.group.config.item_count);
        for (int row = 0; row < GROUPS[group].count; row++) {
            CHECK_EQUAL(0, strcmp(GROUPS[group].labels[row], screens.rows[row].label));
        }
    }
}

static void every_row_but_rescan_opens_a_screen(void)
{
    start();
    for (int group = 0; group < SETTINGS_GROUP_COUNT; group++) {
        open_group((SettingsGroup)group);
        for (int row = 0; row < GROUPS[group].count; row++) {
            bool rescan = strcmp("Rescan library", screens.rows[row].label) == 0;
            CHECK_EQUAL(!rescan, screens.rows[row].opens);
        }
    }
}

static void a_group_remembers_its_selected_row(void)
{
    start();
    open_group(SETTINGS_DISPLAY);
    list_view_select(&screens.group.list, 5, 0);
    open_group(SETTINGS_LIBRARY);
    CHECK_EQUAL(0, screens.group.list.selected);
    list_view_select(&screens.group.list, 2, 0);
    open_group(SETTINGS_DISPLAY);
    CHECK_EQUAL(5, screens.group.list.selected);
    open_group(SETTINGS_LIBRARY);
    CHECK_EQUAL(2, screens.group.list.selected);
}

static void starting_again_forgets_the_selected_rows(void)
{
    start();
    open_group(SETTINGS_DISPLAY);
    list_view_select(&screens.group.list, 5, 0);
    open_group(SETTINGS_SOUND);
    start();
    open_group(SETTINGS_DISPLAY);
    CHECK_EQUAL(0, screens.group.list.selected);
}

int main(void)
{
    const TestCase cases[] = {
        TEST_CASE(the_top_level_lists_the_quick_settings_then_the_groups),
        TEST_CASE(each_group_lists_its_rows_in_order),
        TEST_CASE(every_row_but_rescan_opens_a_screen),
        TEST_CASE(a_group_remembers_its_selected_row),
        TEST_CASE(starting_again_forgets_the_selected_rows),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

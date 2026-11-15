#include <string.h>

#include "check.h"
#include "core/hash.h"
#include "default_theme.h"
#include "fake_hal.h"
#include "gfx/text.h"
#include "settings/settings.h"
#include "settings_screens.h"
#include "theme_fixture.h"
#include "theme_store.h"

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

static SettingsStore default_store;

static Screen *start(void)
{
    default_store = (SettingsStore){.settings = settings_defaults()};
    fake_hal_reset();
    return settings_screens_init(&screens, (SettingsContext){.store = &default_store},
                                 &DEFAULT_THEME);
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

static const char *const THEME_NAMES[THEME_COUNT] = {
    "Dark",  "Light",         "Fjord",    "Dusk",      "Neon",      "Phosphor", "Paper",
    "Olive", "High contrast", "Cassette", "VU meters", "Turntable", "LCD deck"};

static void start_with_theme(SettingsStore *store, int theme)
{
    *store = (SettingsStore){.settings = settings_defaults()};
    store->settings.options[OPTION_THEME] = (uint8_t)theme;
    fake_hal_reset();
    theme_store_start(theme);
    static Player player;
    static Toast toast;
    SettingsContext context = {.store = store, .player = &player, .toast = &toast};
    settings_screens_init(&screens, context, &DEFAULT_THEME);
    open_group(SETTINGS_DISPLAY);
}

static int shown_display_rows(void)
{
    return screens.group.model.count(screens.group.model.context);
}

static bool row_is_shown(int row)
{
    const MenuItem *item = &screens.rows[row];
    return item->shown == NULL || item->shown(item->shown_context);
}

static void the_theme_choice_lists_every_theme_in_stored_order(void)
{
    SettingsStore store;
    start_with_theme(&store, THEME_DARK);
    screens.rows[0].select(screens.rows[0].context);
    CHECK_EQUAL(THEME_COUNT, screens.choice.config.option_count);
    for (int theme = 0; theme < THEME_COUNT; theme++) {
        CHECK_EQUAL(0, strcmp(THEME_NAMES[theme], screens.choice.config.options[theme]));
    }
}

static void open_theme_choice(void)
{
    screens.rows[0].select(screens.rows[0].context);
}

static void choose_theme_option(int option)
{
    screens.choice.config.choose(screens.choice.config.context, option);
}

static void the_theme_choice_lists_theme_files_after_the_built_in_themes(void)
{
    SettingsStore store;
    uint32_t size = 0;
    start_with_theme(&store, THEME_DARK);
    theme_fixture_on_card("ember.echotheme", "Ember.echotheme", &size);
    theme_fixture_on_card("dark.echotheme", "Night Owl.ECHOTHEME", &size);
    fake_hal_add_directory_entry("/Themes", "notes.txt", false, false);
    fake_hal_add_directory_entry("/Themes", "Hidden.echotheme", false, true);
    fake_hal_add_directory_entry("/Themes", "Folder.echotheme", true, false);
    fake_hal_add_directory_entry("/Themes", "A name far too long to show.echotheme", false, false);
    fake_hal_add_directory_entry("/Themes", ".echotheme", false, false);
    open_theme_choice();
    CHECK_EQUAL(THEME_COUNT + 2, screens.choice.config.option_count);
    CHECK_EQUAL(0, strcmp("Ember", screens.choice.config.options[THEME_COUNT]));
    CHECK_EQUAL(0, strcmp("Night Owl", screens.choice.config.options[THEME_COUNT + 1]));
    CHECK_EQUAL(THEME_DARK, screens.choice.config.current(screens.choice.config.context));
}

static void a_card_without_a_themes_folder_lists_only_the_built_in_themes(void)
{
    SettingsStore store;
    start_with_theme(&store, THEME_FJORD);
    open_theme_choice();
    CHECK_EQUAL(THEME_COUNT, screens.choice.config.option_count);
    CHECK_EQUAL(THEME_FJORD, screens.choice.config.current(screens.choice.config.context));
}

static void a_chosen_theme_file_is_worn_named_and_remembered(void)
{
    SettingsStore store;
    uint32_t size = 0;
    start_with_theme(&store, THEME_LIGHT);
    theme_fixture_on_card("ember.echotheme", "Ember.echotheme", &size);
    open_theme_choice();
    uint8_t revision = theme_store_revision();
    choose_theme_option(THEME_COUNT);
    const ThemePackage *package = theme_store_package();
    CHECK(package != NULL);
    CHECK(theme_store_revision() != revision);
    CHECK_EQUAL(COLOR_RGB(0x18, 0x12, 0x10), package->colors.background);
    CHECK_EQUAL(THEME_COUNT, screens.choice.config.current(screens.choice.config.context));
    CHECK_EQUAL(0, strcmp("Ember", screens.rows[0].value(screens.rows[0].context)));
    CHECK_EQUAL(THEME_LIGHT, store.settings.options[OPTION_THEME]);
    const uint8_t *record = fake_hal_file_bytes(THEME_STORE_PATH, &size);
    CHECK(record != NULL && size >= 28);
    CHECK_EQUAL(0, memcmp(record, "ETHS", 4));
    CHECK_EQUAL(0, strcmp("Ember", (const char *)record + 4));
}

static void a_theme_file_without_accents_hides_the_accent_row(void)
{
    SettingsStore store;
    uint32_t size = 0;
    start_with_theme(&store, THEME_DARK);
    CHECK(row_is_shown(1));
    CHECK(row_is_shown(2));
    theme_fixture_on_card("ember.echotheme", "Ember.echotheme", &size);
    open_theme_choice();
    choose_theme_option(THEME_COUNT);
    CHECK(!row_is_shown(1));
    CHECK(row_is_shown(2));
}

static void choosing_a_built_in_theme_takes_the_theme_file_off(void)
{
    SettingsStore store;
    uint32_t size = 0;
    start_with_theme(&store, THEME_DARK);
    theme_fixture_on_card("ember.echotheme", "Ember.echotheme", &size);
    open_theme_choice();
    choose_theme_option(THEME_COUNT);
    choose_theme_option(THEME_NEON);
    CHECK(!theme_store_card());
    CHECK_EQUAL(0, strcmp("Neon", theme_store_package()->name));
    CHECK_EQUAL(THEME_NEON, store.settings.options[OPTION_THEME]);
    CHECK_EQUAL(THEME_NEON, screens.choice.config.current(screens.choice.config.context));
    const uint8_t *record = fake_hal_file_bytes(THEME_STORE_PATH, &size);
    CHECK(record != NULL && record[4] == 0);
}

static void a_damaged_theme_file_is_not_worn(void)
{
    SettingsStore store;
    uint32_t size = 0;
    start_with_theme(&store, THEME_DARK);
    uint8_t *bytes = theme_fixture_on_card("ember.echotheme", "Ember.echotheme", &size);
    bytes[60] ^= 0x40;
    open_theme_choice();
    choose_theme_option(THEME_COUNT);
    CHECK(!theme_store_card());
    CHECK(row_is_shown(1));
    CHECK_EQUAL(THEME_DARK, screens.choice.config.current(screens.choice.config.context));
}

static void the_accent_row_shows_only_on_dark_and_light(void)
{
    SettingsStore store;
    for (int theme = 0; theme < THEME_COUNT; theme++) {
        start_with_theme(&store, theme);
        CHECK_EQUAL(theme == THEME_DARK || theme == THEME_LIGHT, row_is_shown(1));
    }
}

static void the_highlight_row_shows_only_on_gradient_themes(void)
{
    SettingsStore store;
    for (int theme = 0; theme < THEME_COUNT; theme++) {
        start_with_theme(&store, theme);
        bool gradient = theme == THEME_DARK || theme == THEME_LIGHT || theme == THEME_FJORD ||
                        theme == THEME_DUSK || theme == THEME_PAPER || theme >= THEME_CASSETTE;
        CHECK_EQUAL(gradient, row_is_shown(2));
    }
}

static void rename_accent(uint8_t *bytes, uint32_t size, int accent, const char *name)
{
    ThemePackage package = {0};
    CHECK(theme_package_decode(bytes, size, &package));
    uint8_t *entry = bytes + package.accents_at + accent * THEME_PACKAGE_ACCENT_BYTES;
    entry[6] = THEME_ACCENT_CUSTOM;
    memset(entry + THEME_ACCENT_NAME_FIELD, 0, THEME_ACCENT_NAME_BYTES);
    memcpy(entry + THEME_ACCENT_NAME_FIELD, name, strlen(name));
    uint32_t core = (uint32_t)bytes[8] | (uint32_t)bytes[9] << 8 | (uint32_t)bytes[10] << 16 |
                    (uint32_t)bytes[11] << 24;
    uint32_t sum = hash_update(HASH_START, bytes + 16, core - 16);
    for (int index = 0; index < 4; index++) {
        bytes[12 + index] = (uint8_t)(sum >> (8 * index));
    }
}

static void open_accent_choice(void)
{
    screens.rows[1].select(screens.rows[1].context);
}

static void the_accent_choice_lists_the_accents_the_theme_offers(void)
{
    SettingsStore store;
    uint32_t size = 0;
    start_with_theme(&store, THEME_DARK);
    open_accent_choice();
    CHECK_EQUAL(8, screens.choice.config.option_count);
    CHECK_EQUAL(0, strcmp("Blue", screens.choice.config.options[0]));
    CHECK_EQUAL(0, strcmp("Graphite", screens.choice.config.options[7]));
    uint8_t *bytes = theme_fixture_on_card("light.echotheme", "Mine.echotheme", &size);
    rename_accent(bytes, size, 2, "Ruby");
    open_theme_choice();
    choose_theme_option(THEME_COUNT);
    CHECK(theme_store_card());
    open_accent_choice();
    CHECK_EQUAL(0, strcmp("Red", screens.choice.config.options[1]));
    CHECK_EQUAL(0, strcmp("Ruby", screens.choice.config.options[2]));
    screens.choice.config.choose(screens.choice.config.context, 2);
    CHECK_EQUAL(2, store.settings.options[OPTION_ACCENT]);
    CHECK_EQUAL(0, strcmp("Ruby", screens.rows[1].value(screens.rows[1].context)));
}

static bool shown_display_row_is(int position, const char *label)
{
    return strcmp(label, screens.group.model.label(screens.group.model.context, position)) == 0;
}

static void hidden_rows_leave_the_other_display_rows_in_place(void)
{
    SettingsStore store;
    start_with_theme(&store, THEME_DARK);
    CHECK_EQUAL(10, shown_display_rows());
    start_with_theme(&store, THEME_NEON);
    CHECK_EQUAL(8, shown_display_rows());
    CHECK(shown_display_row_is(0, "Theme"));
    CHECK(shown_display_row_is(1, "Brightness"));
    start_with_theme(&store, THEME_FJORD);
    CHECK_EQUAL(9, shown_display_rows());
    CHECK(shown_display_row_is(1, "Highlight"));
    CHECK(shown_display_row_is(2, "Brightness"));
}

int main(void)
{
    const TestCase cases[] = {
        TEST_CASE(the_top_level_lists_the_quick_settings_then_the_groups),
        TEST_CASE(each_group_lists_its_rows_in_order),
        TEST_CASE(every_row_but_rescan_opens_a_screen),
        TEST_CASE(a_group_remembers_its_selected_row),
        TEST_CASE(starting_again_forgets_the_selected_rows),
        TEST_CASE(the_theme_choice_lists_every_theme_in_stored_order),
        TEST_CASE(the_theme_choice_lists_theme_files_after_the_built_in_themes),
        TEST_CASE(a_card_without_a_themes_folder_lists_only_the_built_in_themes),
        TEST_CASE(a_chosen_theme_file_is_worn_named_and_remembered),
        TEST_CASE(a_theme_file_without_accents_hides_the_accent_row),
        TEST_CASE(choosing_a_built_in_theme_takes_the_theme_file_off),
        TEST_CASE(a_damaged_theme_file_is_not_worn),
        TEST_CASE(the_accent_row_shows_only_on_dark_and_light),
        TEST_CASE(the_highlight_row_shows_only_on_gradient_themes),
        TEST_CASE(the_accent_choice_lists_the_accents_the_theme_offers),
        TEST_CASE(hidden_rows_leave_the_other_display_rows_in_place),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

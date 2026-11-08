#include <string.h>

#include "check.h"
#include "default_theme.h"
#include "fake_hal.h"
#include "gfx/text.h"
#include "settings/settings.h"
#include "settings_screens.h"

enum {
    TOP_GROUPS = 5
};

static SettingsScreens screens;

static const char *const TOP[SETTINGS_TOP_COUNT] = {
    "Shuffle", "Repeat",  "Bluetooth", "Equalizer", "Sleep timer",
    "Sound",   "Display", "Library",   "Buttons",   "System"};

static const char *const SOUND[] = {"Gain",
                                    "Output",
                                    "Filter",
                                    "Balance",
                                    "Bluetooth level",
                                    "Book & podcast speed",
                                    "Book & podcast pitch"};
static const char *const DISPLAY[] = {
    "Theme",           "Accent color",        "Highlight",   "Now Playing",
    "Brightness",      "Screen timeout",      "Orientation", "Animations",
    "Battery percent", "Portrait status bar", "Clock"};
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

static void the_speed_rows_show_their_values(void)
{
    SettingsStore store = {.settings = settings_defaults()};
    fake_hal_reset();
    settings_screens_init(&screens, (SettingsContext){.store = &store}, &DEFAULT_THEME);
    open_group(SETTINGS_SOUND);
    CHECK_EQUAL(0, strcmp("1x", screens.rows[5].value(screens.rows[5].context)));
    CHECK_EQUAL(0, strcmp("Keep pitch", screens.rows[6].value(screens.rows[6].context)));
    store.settings.options[OPTION_SPOKEN_SPEED] = SPOKEN_SPEED_0_75X;
    store.settings.options[OPTION_SPOKEN_PITCH] = SPOKEN_PITCH_FOLLOWS_SPEED;
    CHECK_EQUAL(0, strcmp("0.75x", screens.rows[5].value(screens.rows[5].context)));
    CHECK_EQUAL(0, strcmp("Follows speed", screens.rows[6].value(screens.rows[6].context)));
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
    "Dark", "Light", "Fjord", "Dusk", "Neon", "Phosphor", "Paper", "Olive", "High contrast"};

static void start_with_theme(SettingsStore *store, int theme)
{
    *store = (SettingsStore){.settings = settings_defaults()};
    store->settings.options[OPTION_THEME] = (uint8_t)theme;
    fake_hal_reset();
    settings_screens_init(&screens, (SettingsContext){.store = store}, &DEFAULT_THEME);
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
        CHECK_EQUAL(0,
                    strcmp(THEME_NAMES[theme], i18n_text(screens.choice.config.option_ids[theme])));
    }
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
                        theme == THEME_DUSK || theme == THEME_PAPER;
        CHECK_EQUAL(gradient, row_is_shown(2));
    }
}

static bool shown_display_row_is(int position, const char *label)
{
    return strcmp(label, screens.group.model.label(screens.group.model.context, position)) == 0;
}

static void hidden_rows_leave_the_other_display_rows_in_place(void)
{
    SettingsStore store;
    start_with_theme(&store, THEME_DARK);
    CHECK_EQUAL(11, shown_display_rows());
    start_with_theme(&store, THEME_NEON);
    CHECK_EQUAL(9, shown_display_rows());
    CHECK(shown_display_row_is(0, "Theme"));
    CHECK(shown_display_row_is(1, "Now Playing"));
    CHECK(shown_display_row_is(2, "Brightness"));
    start_with_theme(&store, THEME_FJORD);
    CHECK_EQUAL(10, shown_display_rows());
    CHECK(shown_display_row_is(1, "Highlight"));
    CHECK(shown_display_row_is(2, "Now Playing"));
    CHECK(shown_display_row_is(3, "Brightness"));
}

static int now_playing_position(int theme)
{
    SettingsStore store;
    start_with_theme(&store, theme);
    for (int position = 0; position < shown_display_rows(); position++) {
        if (shown_display_row_is(position, "Now Playing")) {
            return position;
        }
    }
    return -1;
}

static void the_now_playing_row_follows_the_rows_the_theme_hides(void)
{
    CHECK_EQUAL(3, now_playing_position(THEME_DARK));
    CHECK_EQUAL(3, now_playing_position(THEME_LIGHT));
    CHECK_EQUAL(2, now_playing_position(THEME_FJORD));
    CHECK_EQUAL(2, now_playing_position(THEME_DUSK));
    CHECK_EQUAL(2, now_playing_position(THEME_PAPER));
    CHECK_EQUAL(1, now_playing_position(THEME_NEON));
    CHECK_EQUAL(1, now_playing_position(THEME_PHOSPHOR));
    CHECK_EQUAL(1, now_playing_position(THEME_OLIVE));
    CHECK_EQUAL(1, now_playing_position(THEME_HIGH_CONTRAST));
}

static void the_now_playing_row_lists_every_face_and_shows_the_chosen_one(void)
{
    static const char *const NAMES[NP_FACE_COUNT] = {"Standard", "Cassette", "VU meters",
                                                     "Turntable", "LCD deck"};
    SettingsStore store;
    start_with_theme(&store, THEME_DARK);
    const MenuItem *row = &screens.rows[3];
    row->select(row->context);
    CHECK_EQUAL(NP_FACE_COUNT, screens.choice.config.option_count);
    for (int face = 0; face < NP_FACE_COUNT; face++) {
        CHECK_EQUAL(0, strcmp(NAMES[face], i18n_text(screens.choice.config.option_ids[face])));
        store.settings.options[OPTION_NOW_PLAYING_FACE] = (uint8_t)face;
        CHECK_EQUAL(0, strcmp(NAMES[face], row->value(row->context)));
    }
}

static void every_face_name_fits_the_value_column(void)
{
    enum {
        VALUE_COLUMN_PX = 88
    };
    SettingsStore store;
    start_with_theme(&store, THEME_DARK);
    const MenuItem *row = &screens.rows[3];
    row->select(row->context);
    for (int face = 0; face < NP_FACE_COUNT; face++) {
        const char *name = i18n_text(screens.choice.config.option_ids[face]);
        CHECK(text_width(DEFAULT_THEME.fonts.small, name) <= VALUE_COLUMN_PX);
    }
}

int main(void)
{
    const TestCase cases[] = {
        TEST_CASE(the_top_level_lists_the_quick_settings_then_the_groups),
        TEST_CASE(each_group_lists_its_rows_in_order),
        TEST_CASE(every_row_but_rescan_opens_a_screen),
        TEST_CASE(the_speed_rows_show_their_values),
        TEST_CASE(a_group_remembers_its_selected_row),
        TEST_CASE(starting_again_forgets_the_selected_rows),
        TEST_CASE(the_theme_choice_lists_every_theme_in_stored_order),
        TEST_CASE(the_accent_row_shows_only_on_dark_and_light),
        TEST_CASE(the_highlight_row_shows_only_on_gradient_themes),
        TEST_CASE(hidden_rows_leave_the_other_display_rows_in_place),
        TEST_CASE(the_now_playing_row_follows_the_rows_the_theme_hides),
        TEST_CASE(the_now_playing_row_lists_every_face_and_shows_the_chosen_one),
        TEST_CASE(every_face_name_fits_the_value_column),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

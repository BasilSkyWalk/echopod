#include <stdio.h>
#include <string.h>

#include "check.h"
#include "default_theme.h"
#include "equalizer_store.h"
#include "fake_hal.h"
#include "screens/equalizer_screen.h"

enum {
    START_MS = 1000
};

static EqualizerStore store;
static Toast toast;
static EqualizerScreen equalizer;

static Screen *open_picker(void)
{
    fake_hal_reset();
    equalizer_store_load(&store);
    toast = (Toast){0};
    Screen *picker = equalizer_screen_init(
        &equalizer, (EqualizerConfig){.store = &store, .toast = &toast, .theme = &DEFAULT_THEME});
    ScreenAction open = equalizer_screen_open(&equalizer);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, open.type);
    CHECK(open.target == picker);
    picker->enter(picker->context, START_MS);
    return picker;
}

static ScreenAction press(Screen *screen, Key key, KeyGesture gesture)
{
    return screen->handle_key(screen->context, (KeyEvent){.key = key, .gesture = gesture},
                              START_MS);
}

static ScreenAction tap(Screen *screen, Key key)
{
    return press(screen, key, KEY_GESTURE_PRESS);
}

static Screen *open_editor(void)
{
    Screen *picker = open_picker();
    for (int step = 0; step < EQUALIZER_PRESET_CUSTOM; step++) {
        tap(picker, KEY_VOLUME_UP);
    }
    ScreenAction action = tap(picker, KEY_PLAY);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, action.type);
    CHECK(action.target == &equalizer.editor.screen);
    action.target->enter(action.target->context, START_MS);
    return action.target;
}

static EqualizerChoice saved_choice(void)
{
    uint32_t size = 0;
    const uint8_t *bytes = fake_hal_file_bytes(EQUALIZER_STORE_PATH, &size);
    EqualizerChoice choice = equalizer_file_defaults();
    CHECK(bytes != NULL);
    CHECK(bytes != NULL && equalizer_file_decode(bytes, size, &choice));
    return choice;
}

static void the_picker_lists_the_stock_presets_and_custom(void)
{
    open_picker();
    CHECK(strcmp("Off", equalizer_screen_summary(&equalizer)) == 0);
    CHECK(strcmp("Retro", equalizer_screen_preset_name(EQUALIZER_PRESET_RETRO)) == 0);
    CHECK(strcmp("Unique", equalizer_screen_preset_name(EQUALIZER_PRESET_UNIQUE)) == 0);
    CHECK(strcmp("Custom", equalizer_screen_preset_name(EQUALIZER_PRESET_CUSTOM)) == 0);
}

static void moving_through_the_list_plays_each_preset(void)
{
    Screen *picker = open_picker();
    tap(picker, KEY_VOLUME_UP);
    CHECK_EQUAL(EQUALIZER_PRESET_RETRO, store.choice.preset);
    CHECK_EQUAL(4, fake_hal_equalizer().band_count);
    tap(picker, KEY_VOLUME_UP);
    CHECK_EQUAL(EQUALIZER_PRESET_BASS, store.choice.preset);
    CHECK_EQUAL(63, fake_hal_equalizer().bands[0].frequency_hz);
    CHECK(strcmp("Bass", equalizer_screen_summary(&equalizer)) == 0);
}

static void leaving_the_picker_saves_what_is_playing(void)
{
    Screen *picker = open_picker();
    tap(picker, KEY_VOLUME_UP);
    tap(picker, KEY_VOLUME_UP);
    tap(picker, KEY_VOLUME_UP);
    CHECK_EQUAL(SCREEN_ACTION_POP, tap(picker, KEY_BACK).type);
    CHECK_EQUAL(EQUALIZER_PRESET_HEAVY, saved_choice().preset);
}

static void choosing_a_preset_closes_the_picker(void)
{
    Screen *picker = open_picker();
    tap(picker, KEY_VOLUME_UP);
    CHECK_EQUAL(SCREEN_ACTION_POP, tap(picker, KEY_PLAY).type);
    CHECK_EQUAL(EQUALIZER_PRESET_RETRO, saved_choice().preset);
}

static void the_editor_opens_picking_the_first_band(void)
{
    open_editor();
    CHECK_EQUAL(EQUALIZER_PRESET_CUSTOM, store.choice.preset);
    CHECK_EQUAL(0, equalizer.editor.band);
    CHECK(!equalizer.editor.editing);
}

static void volume_moves_between_bands_both_ways_and_stops_at_the_ends(void)
{
    Screen *editor = open_editor();
    tap(editor, KEY_VOLUME_UP);
    tap(editor, KEY_VOLUME_UP);
    CHECK_EQUAL(2, equalizer.editor.band);
    tap(editor, KEY_VOLUME_DOWN);
    CHECK_EQUAL(1, equalizer.editor.band);
    for (int step = 0; step < 10; step++) {
        tap(editor, KEY_VOLUME_DOWN);
    }
    CHECK_EQUAL(0, equalizer.editor.band);
    for (int step = 0; step < 10; step++) {
        tap(editor, KEY_VOLUME_UP);
    }
    CHECK_EQUAL(EQUALIZER_CUSTOM_BANDS - 1, equalizer.editor.band);
    for (int band = 0; band < EQUALIZER_CUSTOM_BANDS; band++) {
        CHECK_EQUAL(0, store.choice.custom_gains[band]);
    }
}

static void play_grabs_a_band_and_volume_changes_its_level(void)
{
    Screen *editor = open_editor();
    tap(editor, KEY_PLAY);
    CHECK(equalizer.editor.editing);
    tap(editor, KEY_VOLUME_UP);
    press(editor, KEY_VOLUME_UP, KEY_GESTURE_REPEAT);
    CHECK_EQUAL(2, store.choice.custom_gains[0]);
    CHECK_EQUAL(20, fake_hal_equalizer().bands[0].gain_tenths);
    CHECK(strcmp("63 Hz   +2 dB", equalizer.editor.band_text) == 0);
    CHECK(strcmp("Preamp -2 dB", equalizer.editor.preamp_text) == 0);
    tap(editor, KEY_PLAY);
    CHECK(!equalizer.editor.editing);
    tap(editor, KEY_VOLUME_UP);
    tap(editor, KEY_PLAY);
    tap(editor, KEY_VOLUME_DOWN);
    CHECK_EQUAL(-1, store.choice.custom_gains[1]);
    CHECK_EQUAL(2, store.choice.custom_gains[0]);
    CHECK(strcmp("160 Hz   -1 dB", equalizer.editor.band_text) == 0);
}

static void a_missed_band_is_one_press_back(void)
{
    Screen *editor = open_editor();
    tap(editor, KEY_VOLUME_UP);
    tap(editor, KEY_VOLUME_UP);
    tap(editor, KEY_VOLUME_UP);
    tap(editor, KEY_VOLUME_DOWN);
    tap(editor, KEY_PLAY);
    tap(editor, KEY_VOLUME_UP);
    CHECK_EQUAL(1, store.choice.custom_gains[2]);
}

static void the_level_stops_at_twelve_decibels(void)
{
    Screen *editor = open_editor();
    tap(editor, KEY_PLAY);
    for (int step = 0; step < 20; step++) {
        tap(editor, KEY_VOLUME_UP);
    }
    CHECK_EQUAL(EQUALIZER_GAIN_MAX_DB, store.choice.custom_gains[0]);
    for (int step = 0; step < 40; step++) {
        tap(editor, KEY_VOLUME_DOWN);
    }
    CHECK_EQUAL(EQUALIZER_GAIN_MIN_DB, store.choice.custom_gains[0]);
}

static void holding_play_flattens_the_band_in_either_mode(void)
{
    Screen *editor = open_editor();
    tap(editor, KEY_PLAY);
    tap(editor, KEY_VOLUME_UP);
    tap(editor, KEY_VOLUME_UP);
    press(editor, KEY_PLAY, KEY_GESTURE_LONG_PRESS);
    CHECK_EQUAL(0, store.choice.custom_gains[0]);
    tap(editor, KEY_VOLUME_DOWN);
    tap(editor, KEY_PLAY);
    press(editor, KEY_PLAY, KEY_GESTURE_LONG_PRESS);
    CHECK_EQUAL(0, store.choice.custom_gains[0]);
}

static void back_lets_go_of_a_band_before_it_leaves(void)
{
    Screen *editor = open_editor();
    tap(editor, KEY_PLAY);
    CHECK_EQUAL(SCREEN_ACTION_NONE, tap(editor, KEY_BACK).type);
    CHECK(!equalizer.editor.editing);
    CHECK_EQUAL(SCREEN_ACTION_POP, tap(editor, KEY_BACK).type);
}

static void leaving_the_editor_saves_the_custom_curve(void)
{
    Screen *editor = open_editor();
    tap(editor, KEY_VOLUME_UP);
    tap(editor, KEY_VOLUME_UP);
    tap(editor, KEY_VOLUME_UP);
    tap(editor, KEY_PLAY);
    tap(editor, KEY_VOLUME_UP);
    tap(editor, KEY_BACK);
    CHECK_EQUAL(SCREEN_ACTION_POP, tap(editor, KEY_BACK).type);
    EqualizerChoice choice = saved_choice();
    CHECK_EQUAL(EQUALIZER_PRESET_CUSTOM, choice.preset);
    CHECK_EQUAL(1, choice.custom_gains[3]);
}

static void holding_back_saves_and_goes_home_while_adjusting(void)
{
    Screen *editor = open_editor();
    tap(editor, KEY_PLAY);
    tap(editor, KEY_VOLUME_UP);
    CHECK_EQUAL(SCREEN_ACTION_POP_TO_ROOT, press(editor, KEY_BACK, KEY_GESTURE_LONG_PRESS).type);
    CHECK_EQUAL(1, saved_choice().custom_gains[0]);
}

static void the_saved_choice_comes_back_at_start(void)
{
    Screen *editor = open_editor();
    tap(editor, KEY_PLAY);
    tap(editor, KEY_VOLUME_DOWN);
    tap(editor, KEY_BACK);
    tap(editor, KEY_BACK);
    EqualizerStore reloaded;
    equalizer_store_load(&reloaded);
    equalizer_store_apply(&reloaded);
    CHECK_EQUAL(EQUALIZER_PRESET_CUSTOM, reloaded.choice.preset);
    CHECK_EQUAL(-1, reloaded.choice.custom_gains[0]);
    CHECK_EQUAL(EQUALIZER_CUSTOM_BANDS, fake_hal_equalizer().band_count);
    CHECK_EQUAL(-10, fake_hal_equalizer().bands[0].gain_tenths);
}

static void an_unchanged_choice_is_not_written_again(void)
{
    Screen *picker = open_picker();
    tap(picker, KEY_VOLUME_UP);
    tap(picker, KEY_BACK);
    int writes = fake_hal_write_count();
    picker->enter(picker->context, START_MS);
    tap(picker, KEY_BACK);
    CHECK_EQUAL(writes, fake_hal_write_count());
}

static const char PRESET_A[] = "Preamp: -1 dB\n"
                               "Filter 1: ON PK Fc 1000 Hz Gain 2 dB Q 1.5\n";
static const char PRESET_B[] = "Preamp: -2 dB\n"
                               "Filter 1: ON PK Fc 2000 Hz Gain -3 dB Q 0.8\n";
static const char PRESET_BAD[] = "Filter 1: ON PK Fc 100 Hz Gain 13 dB Q 0.7\n";
static const char PRESET_SKIPPING[] = "Preamp: -2 dB\n"
                                      "Filter 1: ON LP Fc 18000 Hz Q 0.7\n"
                                      "Filter 2: ON PK Fc 1000 Hz Gain -2 dB Q 1\n"
                                      "Filter 3: ON HP Fc 20 Hz\n";

static void add_preset(const char *name, const char *text)
{
    char path[64];
    snprintf(path, sizeof path, "/EQ/%s", name);
    fake_hal_add_file(path, (const uint8_t *)text, (uint32_t)strlen(text));
}

static Screen *open_files(void)
{
    fake_hal_reset();
    fake_hal_add_directory_entry("/EQ", "A.txt", false, false);
    add_preset("A.txt", PRESET_A);
    equalizer_store_load(&store);
    toast = (Toast){0};
    Screen *picker = equalizer_screen_init(
        &equalizer, (EqualizerConfig){.store = &store, .toast = &toast, .theme = &DEFAULT_THEME});
    equalizer_screen_open(&equalizer);
    picker->enter(picker->context, START_MS);
    for (int step = 0; step < EQUALIZER_PRESET_FILE; step++) {
        tap(picker, KEY_VOLUME_UP);
    }
    return picker;
}

static void the_file_list_lists_txt_presets(void)
{
    open_files();
    CHECK_EQUAL(1, equalizer.files.count);
    CHECK(strcmp("A", equalizer.model.label(&equalizer, EQUALIZER_PRESET_FILE)) == 0);
    CHECK_EQUAL(EQUALIZER_PRESET_FILE, equalizer.list.selected);
}

static void the_settings_summary_uses_seven_characters_and_the_list_the_whole_name(void)
{
    fake_hal_reset();
    fake_hal_add_directory_entry("/EQ", "LongPresetName.txt", false, false);
    add_preset("LongPresetName.txt", PRESET_A);
    equalizer_store_load(&store);
    toast = (Toast){0};
    equalizer_screen_init(
        &equalizer, (EqualizerConfig){.store = &store, .toast = &toast, .theme = &DEFAULT_THEME});
    equalizer_screen_open(&equalizer);
    EqualizerTextError error;
    CHECK(equalizer_store_select_file(&store, "LongPresetName.txt", &error));
    CHECK_EQUAL(1, equalizer.files.count);
    CHECK(strcmp("LongPre", equalizer_screen_summary(&equalizer)) == 0);
    CHECK(strcmp("LongPresetName", equalizer.model.label(&equalizer, EQUALIZER_PRESET_FILE)) == 0);
    CHECK(equalizer.model.detail(&equalizer, EQUALIZER_PRESET_FILE) != NULL);
}

static void opening_the_picker_finds_files_added_since_the_last_time(void)
{
    Screen *picker = open_files();
    tap(picker, KEY_BACK);
    fake_hal_add_directory_entry("/EQ", "B.txt", false, false);
    add_preset("B.txt", PRESET_B);
    equalizer_screen_open(&equalizer);
    picker->enter(picker->context, START_MS);
    CHECK_EQUAL(2, equalizer.files.count);
    CHECK(strcmp("B", equalizer.model.label(&equalizer, EQUALIZER_PRESET_FILE + 1)) == 0);
}

static void a_rejected_file_says_why(void)
{
    fake_hal_reset();
    fake_hal_add_directory_entry("/EQ", "Bad.txt", false, false);
    add_preset("Bad.txt", PRESET_BAD);
    equalizer_store_load(&store);
    toast = (Toast){0};
    Screen *picker = equalizer_screen_init(
        &equalizer, (EqualizerConfig){.store = &store, .toast = &toast, .theme = &DEFAULT_THEME});
    equalizer_screen_open(&equalizer);
    picker->enter(picker->context, START_MS);
    for (int step = 0; step < EQUALIZER_PRESET_FILE; step++) {
        tap(picker, KEY_VOLUME_UP);
    }
    CHECK(toast.visible);
    CHECK(strcmp("Line 1: gain over 12 dB", equalizer.notice) == 0);
}

static void entering_the_picker_does_not_rescan_the_card(void)
{
    Screen *picker = open_files();
    int opens = fake_hal_dir_open_count();
    picker->enter(picker->context, START_MS);
    CHECK_EQUAL(opens, fake_hal_dir_open_count());
}

static void choosing_a_valid_file_applies_and_saves_it(void)
{
    Screen *picker = open_files();
    CHECK_EQUAL(SCREEN_ACTION_POP, tap(picker, KEY_PLAY).type);
    CHECK_EQUAL(EQUALIZER_PRESET_FILE, store.choice.preset);
    CHECK(strcmp("A.txt", store.choice.preset_name) == 0);
    CHECK_EQUAL(1, fake_hal_equalizer().band_count);
    CHECK_EQUAL(1000, fake_hal_equalizer().bands[0].frequency_hz);
    CHECK_EQUAL(20, fake_hal_equalizer().bands[0].gain_tenths);
}

static void highlighting_a_file_previews_it(void)
{
    fake_hal_reset();
    fake_hal_add_directory_entry("/EQ", "A.txt", false, false);
    fake_hal_add_directory_entry("/EQ", "B.txt", false, false);
    add_preset("A.txt", PRESET_A);
    add_preset("B.txt", PRESET_B);
    equalizer_store_load(&store);
    toast = (Toast){0};
    Screen *picker = equalizer_screen_init(
        &equalizer, (EqualizerConfig){.store = &store, .toast = &toast, .theme = &DEFAULT_THEME});
    equalizer_screen_open(&equalizer);
    picker->enter(picker->context, START_MS);
    for (int step = 0; step < EQUALIZER_PRESET_FILE; step++) {
        tap(picker, KEY_VOLUME_UP);
    }
    tap(picker, KEY_VOLUME_UP);
    CHECK_EQUAL(EQUALIZER_PRESET_FILE, store.choice.preset);
    CHECK(strcmp("B.txt", store.choice.preset_name) == 0);
    CHECK_EQUAL(2000, fake_hal_equalizer().bands[0].frequency_hz);
}

static void an_invalid_file_is_not_applied_or_saved(void)
{
    fake_hal_reset();
    fake_hal_add_directory_entry("/EQ", "Bad.txt", false, false);
    add_preset("Bad.txt", PRESET_BAD);
    equalizer_store_load(&store);
    toast = (Toast){0};
    Screen *picker = equalizer_screen_init(
        &equalizer, (EqualizerConfig){.store = &store, .toast = &toast, .theme = &DEFAULT_THEME});
    equalizer_screen_open(&equalizer);
    picker->enter(picker->context, START_MS);
    for (int step = 0; step < EQUALIZER_PRESET_FILE; step++) {
        tap(picker, KEY_VOLUME_UP);
    }
    CHECK_EQUAL(SCREEN_ACTION_NONE, tap(picker, KEY_PLAY).type);
    CHECK_EQUAL(EQUALIZER_PRESET_CUSTOM, store.choice.preset);
    CHECK(toast.visible);
    CHECK_EQUAL(EQUALIZER_CUSTOM_BANDS, fake_hal_equalizer().band_count);
}

static void a_file_with_unsupported_filters_plays_the_rest_and_says_so(void)
{
    fake_hal_reset();
    fake_hal_add_directory_entry("/EQ", "Mixed.txt", false, false);
    add_preset("Mixed.txt", PRESET_SKIPPING);
    equalizer_store_load(&store);
    toast = (Toast){0};
    Screen *picker = equalizer_screen_init(
        &equalizer, (EqualizerConfig){.store = &store, .toast = &toast, .theme = &DEFAULT_THEME});
    equalizer_screen_open(&equalizer);
    picker->enter(picker->context, START_MS);
    for (int step = 0; step < EQUALIZER_PRESET_FILE; step++) {
        tap(picker, KEY_VOLUME_UP);
    }
    CHECK_EQUAL(EQUALIZER_PRESET_FILE, store.choice.preset);
    CHECK_EQUAL(1, fake_hal_equalizer().band_count);
    CHECK(toast.visible);
    CHECK(strcmp("2 filters skipped", equalizer.notice) == 0);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_picker_lists_the_stock_presets_and_custom),
        TEST_CASE(moving_through_the_list_plays_each_preset),
        TEST_CASE(leaving_the_picker_saves_what_is_playing),
        TEST_CASE(choosing_a_preset_closes_the_picker),
        TEST_CASE(the_editor_opens_picking_the_first_band),
        TEST_CASE(volume_moves_between_bands_both_ways_and_stops_at_the_ends),
        TEST_CASE(play_grabs_a_band_and_volume_changes_its_level),
        TEST_CASE(a_missed_band_is_one_press_back),
        TEST_CASE(the_level_stops_at_twelve_decibels),
        TEST_CASE(holding_play_flattens_the_band_in_either_mode),
        TEST_CASE(back_lets_go_of_a_band_before_it_leaves),
        TEST_CASE(leaving_the_editor_saves_the_custom_curve),
        TEST_CASE(holding_back_saves_and_goes_home_while_adjusting),
        TEST_CASE(the_saved_choice_comes_back_at_start),
        TEST_CASE(an_unchanged_choice_is_not_written_again),
        TEST_CASE(the_file_list_lists_txt_presets),
        TEST_CASE(the_settings_summary_uses_seven_characters_and_the_list_the_whole_name),
        TEST_CASE(opening_the_picker_finds_files_added_since_the_last_time),
        TEST_CASE(a_rejected_file_says_why),
        TEST_CASE(entering_the_picker_does_not_rescan_the_card),
        TEST_CASE(choosing_a_valid_file_applies_and_saves_it),
        TEST_CASE(highlighting_a_file_previews_it),
        TEST_CASE(an_invalid_file_is_not_applied_or_saved),
        TEST_CASE(a_file_with_unsupported_filters_plays_the_rest_and_says_so),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

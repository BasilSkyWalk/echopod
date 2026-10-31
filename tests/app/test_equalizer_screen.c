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
    CHECK_EQUAL(2, fake_hal_equalizer().bands[0].gain_db);
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
    CHECK_EQUAL(-1, fake_hal_equalizer().bands[0].gain_db);
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
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

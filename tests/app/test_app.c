#include <string.h>

#include "app/app.h"
#include "app_internal.h"
#include "check.h"
#include "fake_hal.h"
#include "fixture_library.h"
#include "frame_overlay.h"
#include "hal/display.h"

enum {
    START_MS = 1000,
    KEY_HOLD_MS = 30,
    FULL_FRAME_PIXELS = HAL_DISPLAY_WIDTH * HAL_DISPLAY_HEIGHT,
    HOME_DIAGNOSTICS_INDEX = 3,
    DIAGNOSTICS_FRAME_TIME_INDEX = 3,
    MUSIC_ARTISTS_INDEX = 1,
    MUSIC_ALBUMS_INDEX = 3,
    MUSIC_FOLDERS_INDEX = 5,
    BJORK_POSITION = 3,
    HOME_SETTINGS_INDEX = 2,
    SETTINGS_REPEAT_INDEX = 1
};

static const char HELLS_BELLS_PATH[] = "/AC DC/Back In Black/1-01 - Hells Bells.flac";
static const char SHOOT_TO_THRILL_PATH[] = "/AC DC/Back In Black/1-02 - Shoot to Thrill.flac";

static uint32_t now_ms;

static void start_app(void)
{
    fake_hal_reset();
    now_ms = START_MS;
    fake_hal_set_milliseconds(now_ms);
    app_start();
}

static void start_app_with_library(void)
{
    uint32_t size = 0;
    const uint8_t *bytes = fixture_bytes(&size);
    fake_hal_reset();
    fake_hal_add_file("/.echo/library.db", bytes, size);
    now_ms = START_MS;
    fake_hal_set_milliseconds(now_ms);
    app_start();
}

static void step(void)
{
    fake_hal_set_milliseconds(now_ms);
    app_step();
}

static void tap(Key key)
{
    fake_hal_queue_transition(key, true, now_ms);
    fake_hal_queue_transition(key, false, now_ms + KEY_HOLD_MS);
    now_ms += KEY_HOLD_MS + 10;
    step();
}

static void hold_back(void)
{
    fake_hal_queue_transition(KEY_BACK, true, now_ms);
    now_ms += 1000;
    fake_hal_queue_transition(KEY_BACK, false, now_ms);
    step();
}

static void move_down(int times)
{
    for (int count = 0; count < times; count++) {
        tap(KEY_VOLUME_DOWN);
    }
}

static bool title_is(const char *expected)
{
    return strcmp(expected, app_screen_title()) == 0;
}

static bool overlay_visible(void)
{
    Rect area = frame_overlay_area();
    return fake_hal_pixel(area.x, area.y) == frame_overlay_background();
}

static void a_step_writes_one_full_frame_and_presents_it(void)
{
    start_app();
    step();
    CHECK_EQUAL(FULL_FRAME_PIXELS, fake_hal_pixels_written());
    CHECK_EQUAL(1, fake_hal_present_count());
}

static void the_home_menu_is_the_root(void)
{
    start_app();
    step();
    CHECK(title_is("Echo Mini"));
    CHECK_EQUAL(1, app_screen_depth());
}

static void selecting_music_opens_the_music_menu(void)
{
    start_app();
    tap(KEY_PLAY);
    CHECK(title_is("Music"));
    CHECK_EQUAL(2, app_screen_depth());
}

static void back_returns_to_the_previous_screen(void)
{
    start_app();
    tap(KEY_PLAY);
    tap(KEY_BACK);
    CHECK(title_is("Echo Mini"));
}

static void a_long_back_returns_home_from_deep_screens(void)
{
    start_app();
    tap(KEY_PLAY);
    tap(KEY_PLAY);
    CHECK_EQUAL(3, app_screen_depth());
    hold_back();
    CHECK_EQUAL(1, app_screen_depth());
}

static void the_frame_overlay_is_off_until_enabled_in_diagnostics(void)
{
    start_app();
    step();
    CHECK(!overlay_visible());
    move_down(HOME_DIAGNOSTICS_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Diagnostics"));
    move_down(DIAGNOSTICS_FRAME_TIME_INDEX);
    tap(KEY_PLAY);
    CHECK(overlay_visible());
}

static void open_music_entry(int index)
{
    tap(KEY_PLAY);
    move_down(index);
    tap(KEY_PLAY);
}

static void albums_open_from_the_library(void)
{
    start_app_with_library();
    open_music_entry(MUSIC_ALBUMS_INDEX);
    CHECK(title_is("Albums"));
    tap(KEY_PLAY);
    CHECK(title_is("Back In Black"));
    CHECK_EQUAL(4, app_screen_depth());
}

static void an_artist_leads_to_their_albums(void)
{
    start_app_with_library();
    open_music_entry(MUSIC_ARTISTS_INDEX);
    move_down(BJORK_POSITION);
    tap(KEY_PLAY);
    CHECK(title_is("Björk"));
    tap(KEY_PLAY);
    CHECK(title_is("Homogenic"));
}

static void back_returns_to_the_previous_list(void)
{
    start_app_with_library();
    open_music_entry(MUSIC_ALBUMS_INDEX);
    tap(KEY_PLAY);
    tap(KEY_BACK);
    CHECK(title_is("Albums"));
}

static void folders_can_be_browsed_down_to_an_album(void)
{
    start_app_with_library();
    open_music_entry(MUSIC_FOLDERS_INDEX);
    CHECK(title_is("Folders"));
    tap(KEY_PLAY);
    CHECK(title_is("AC DC"));
    tap(KEY_PLAY);
    CHECK(title_is("Back In Black"));
    CHECK_EQUAL(5, app_screen_depth());
}

static void without_a_library_the_lists_explain_what_is_missing(void)
{
    start_app();
    open_music_entry(MUSIC_ARTISTS_INDEX);
    CHECK(title_is("No library"));
}

static bool playing_path_is(const char *expected)
{
    return strcmp(expected, fake_hal_audio_playing_path()) == 0;
}

static void play_first_album_track(void)
{
    open_music_entry(MUSIC_ALBUMS_INDEX);
    tap(KEY_PLAY);
    tap(KEY_PLAY);
}

static void selecting_a_track_plays_it_and_shows_now_playing(void)
{
    start_app_with_library();
    play_first_album_track();
    CHECK(title_is("Now Playing"));
    CHECK(playing_path_is(HELLS_BELLS_PATH));
}

static void now_playing_keys_control_the_player(void)
{
    start_app_with_library();
    play_first_album_track();
    uint8_t volume = fake_hal_audio_volume();
    tap(KEY_VOLUME_UP);
    CHECK_EQUAL(volume + 1, fake_hal_audio_volume());
    tap(KEY_PLAY);
    CHECK_EQUAL(PLAYER_PAUSED, app_player_status().state);
    tap(KEY_MENU);
    CHECK(playing_path_is(SHOOT_TO_THRILL_PATH));
}

static void the_home_menu_opens_now_playing(void)
{
    start_app_with_library();
    move_down(1);
    tap(KEY_PLAY);
    CHECK(title_is("Now Playing"));
}

static void settings_cycle_shuffle_and_repeat(void)
{
    start_app_with_library();
    move_down(HOME_SETTINGS_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Settings"));
    tap(KEY_PLAY);
    CHECK_EQUAL(SHUFFLE_SONGS, app_player_status().shuffle);
    move_down(SETTINGS_REPEAT_INDEX);
    tap(KEY_PLAY);
    tap(KEY_PLAY);
    CHECK_EQUAL(REPEAT_ALL, app_player_status().repeat);
}

static void stopping_the_app_saves_the_session(void)
{
    start_app_with_library();
    play_first_album_track();
    int writes = fake_hal_write_count();
    app_stop();
    CHECK_EQUAL(writes + 1, fake_hal_write_count());
}

static void each_step_records_its_duration(void)
{
    start_app();
    fake_hal_set_microseconds_per_read(4000);
    step();
    CHECK_EQUAL(4000, frame_stats_latest_us(app_frame_stats()));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_step_writes_one_full_frame_and_presents_it),
        TEST_CASE(the_home_menu_is_the_root),
        TEST_CASE(selecting_music_opens_the_music_menu),
        TEST_CASE(back_returns_to_the_previous_screen),
        TEST_CASE(a_long_back_returns_home_from_deep_screens),
        TEST_CASE(the_frame_overlay_is_off_until_enabled_in_diagnostics),
        TEST_CASE(albums_open_from_the_library),
        TEST_CASE(an_artist_leads_to_their_albums),
        TEST_CASE(back_returns_to_the_previous_list),
        TEST_CASE(folders_can_be_browsed_down_to_an_album),
        TEST_CASE(without_a_library_the_lists_explain_what_is_missing),
        TEST_CASE(selecting_a_track_plays_it_and_shows_now_playing),
        TEST_CASE(now_playing_keys_control_the_player),
        TEST_CASE(the_home_menu_opens_now_playing),
        TEST_CASE(settings_cycle_shuffle_and_repeat),
        TEST_CASE(stopping_the_app_saves_the_session),
        TEST_CASE(each_step_records_its_duration),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

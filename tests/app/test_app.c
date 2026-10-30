#include <string.h>

#include "app/app.h"
#include "app_internal.h"
#include "check.h"
#include "default_theme.h"
#include "fake_hal.h"
#include "fixture_library.h"
#include "frame_overlay.h"
#include "gfx/text.h"
#include "hal/display.h"
#include "hal/system.h"
#include "library/cover_build.h"
#include "library/covers.h"
#include "library_rescan.h"
#include "settings/settings.h"
#include "settings_store.h"
#include "ui/slide.h"
#include "ui/status_bar.h"
#include "ui/toast.h"

enum {
    START_MS = 1000,
    KEY_HOLD_MS = 30,
    FULL_FRAME_PIXELS = HAL_DISPLAY_WIDTH * HAL_DISPLAY_HEIGHT,
    HOME_DIAGNOSTICS_INDEX = 2,
    DIAGNOSTICS_FRAME_TIME_INDEX = 3,
    DIAGNOSTICS_FIRMWARE_INDEX = 4,
    MUSIC_ARTISTS_INDEX = 1,
    MUSIC_ALBUMS_INDEX = 3,
    MUSIC_FOLDERS_INDEX = 5,
    BJORK_POSITION = 3,
    HOME_SETTINGS_INDEX = 1,
    HOME_NOW_PLAYING_INDEX = 3,
    SETTINGS_REPEAT_INDEX = 1,
    SETTINGS_SLEEP_INDEX = 3,
    SETTINGS_TIMEOUT_INDEX = 5,
    SETTINGS_BRIGHTNESS_INDEX = 6,
    SETTINGS_GAIN_INDEX = 7,
    SETTINGS_START_INDEX = 9,
    SETTINGS_RESCAN_INDEX = 10,
    MUSIC_COVER_FLOW_INDEX = 0,
    MUSIC_PLAYLISTS_INDEX = 7,
    COVER_CENTER_X = 160,
    COVER_CENTER_Y = 60,
    SETTLE_STEPS = 30,
    RED_COVER = 0xF800,
    BLUE_COVER = 0x001F,
    MUSIC_FAVORITES_INDEX = 8,
    DEFAULT_TIMEOUT_MS = 30000,
    SHORT_TIMEOUT_MS = 10000,
    IDLE_MS = 20000,
    DIAGNOSTICS_KEY_TEST_INDEX = 0,
    DIAGNOSTICS_CPU_CLOCK_INDEX = 5,
    DIAGNOSTICS_STOPWATCH_INDEX = 6,
    DIAGNOSTICS_TEST_TONE_INDEX = 7,
    DIAGNOSTICS_DMA_TONE_INDEX = 8,
    DIAGNOSTICS_ACCEL_INDEX = 24,
    DIAGNOSTICS_HIGH_RATE_TONE_INDEX = 25,
    DIAGNOSTICS_WATCHDOG_TEST_INDEX = 26,
    DIAGNOSTICS_TONE_176_INDEX = 27,
    DIAGNOSTICS_LOOP_CYCLES_INDEX = 11,
    DIAGNOSTICS_CORE1_INDEX = 12,
    DIAGNOSTICS_STALL_TEST_INDEX = 19,
    DIAGNOSTICS_POWER_TAPS_INDEX = 20,
    FIFTEEN_MINUTES_MS = 15 * 60 * 1000,
    GOODBYE_MS = 1000,
    SPLASH_FADE_MS = 600,
    SPLASH_MS = 1100,
    HALF_BATTERY = 50
};

static const char HELLS_BELLS_PATH[] = "/AC DC/Back In Black/1-01 - Hells Bells.flac";
static const char SHOOT_TO_THRILL_PATH[] = "/AC DC/Back In Black/1-02 - Shoot to Thrill.flac";
static const char SHOOTS_PATH[] = "/acloudyskye/This Won't Be The Last Time/01 - Shoots.flac";

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
    uint32_t covers_size = 0;
    const uint8_t *bytes = fixture_bytes(&size);
    const uint8_t *covers = fixture_covers_bytes(&covers_size);
    fake_hal_reset();
    fake_hal_add_file("/.echo/library.db", bytes, size);
    fake_hal_add_file("/.echo/covers.bin", covers, covers_size);
    fake_hal_add_file("/.echo/build.tmp", NULL, 16 * 1024 * 1024);
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

static void hold_for(Key key, uint32_t duration_ms)
{
    fake_hal_queue_transition(key, true, now_ms);
    now_ms += duration_ms;
    step();
    fake_hal_queue_transition(key, false, now_ms);
    now_ms += 10;
    step();
}

static void hold(Key key)
{
    hold_for(key, 1000);
}

static void hold_power(void)
{
    hold_for(KEY_POWER, 2000);
}

static void hold_back(void)
{
    hold(KEY_BACK);
}

static void wait_ms(uint32_t duration_ms)
{
    now_ms += duration_ms;
    step();
}

static void restart_app(void)
{
    now_ms += 10;
    fake_hal_set_milliseconds(now_ms);
    app_start();
}

static void move_down(int times)
{
    for (int count = 0; count < times; count++) {
        tap(KEY_VOLUME_UP);
    }
}

static void move_up(int times)
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

static void audio_gets_a_turn_after_every_strip(void)
{
    start_app();
    int before = fake_hal_audio_service_count();
    step();
    CHECK(fake_hal_audio_service_count() - before >= HAL_DISPLAY_HEIGHT / 34 + 1);
}

static void bluetooth_gets_a_turn_after_every_strip(void)
{
    start_app();
    int before = fake_hal_bluetooth_service_count();
    step();
    CHECK(fake_hal_bluetooth_service_count() - before >= HAL_DISPLAY_HEIGHT / 34 + 1);
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
    CHECK(title_is("EchoPod"));
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
    CHECK(title_is("EchoPod"));
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

static void the_frame_overlay_splits_the_frame_time(void)
{
    char text[FRAME_OVERLAY_TEXT_CAPACITY];
    FrameBreakdown breakdown = {
        .total_us = 190400, .update_us = 12000, .draw_us = 119600, .send_us = 58200};
    frame_overlay_format(breakdown, text, sizeof text);
    CHECK(strcmp("190 u12 d120 s58", text) == 0);
}

static void the_frame_overlay_splits_the_drawing(void)
{
    char text[FRAME_OVERLAY_TEXT_CAPACITY];
    uint32_t parts[DRAW_PART_COUNT] = {[DRAW_PART_COVERS] = 9400, [DRAW_PART_REFLECTIONS] = 3100,
                                       [DRAW_PART_TEXT] = 2000,   [DRAW_PART_BACKDROP] = 600,
                                       [DRAW_PART_HASH] = 1500,   [DRAW_PART_AUDIO] = 0};
    frame_overlay_format_parts(parts, text, sizeof text);
    CHECK(strcmp("c9 r3 t2 b1 h2 a0", text) == 0);
}

static void the_frame_overlay_shows_the_audio_numbers(void)
{
    char text[FRAME_OVERLAY_TEXT_CAPACITY];
    HalAudioStats stats = {.buffered_ms = 340,
                           .underruns = 2,
                           .output_underruns = 7,
                           .decode_total_us = 100000,
                           .busy_us = 640000,
                           .elapsed_us = 1000000};
    frame_overlay_format_audio(stats, text, sizeof text);
    CHECK(strcmp("b340 x2 o7 L64%", text) == 0);
}

static void the_frame_overlay_shows_average_and_longest_block_times(void)
{
    char text[FRAME_OVERLAY_TEXT_CAPACITY];
    HalAudioStats stats = {.blocks = 4,
                           .decode_total_us = 92000,
                           .decode_max_us = 38400,
                           .read_total_us = 20000,
                           .read_max_us = 11600};
    frame_overlay_format_block_times(stats, text, sizeof text);
    CHECK(strcmp("d23/38 c5/12", text) == 0);
}

static void the_frame_overlay_shows_zero_before_any_block(void)
{
    char text[FRAME_OVERLAY_TEXT_CAPACITY];
    frame_overlay_format_block_times((HalAudioStats){0}, text, sizeof text);
    CHECK(strcmp("d0/0 c0/0", text) == 0);
    frame_overlay_format_audio((HalAudioStats){0}, text, sizeof text);
    CHECK(strcmp("b0 x0 o0 L0%", text) == 0);
}

static void open_music_entry(int index)
{
    tap(KEY_PLAY);
    move_down(index);
    tap(KEY_PLAY);
}

static bool columns_have_ink(int row, int first, int last)
{
    int top = DEFAULT_THEME.metrics.header_height + row * DEFAULT_THEME.metrics.row_height;
    for (int y = top; y < top + DEFAULT_THEME.metrics.row_height; y++) {
        for (int x = first; x < last; x++) {
            if (fake_hal_pixel(x, y) != DEFAULT_THEME.colors.background) {
                return true;
            }
        }
    }
    return false;
}

static bool row_shows_a_chevron(int row)
{
    int right = HAL_DISPLAY_WIDTH - DEFAULT_THEME.metrics.padding;
    int chevron = text_width(DEFAULT_THEME.fonts.title, "\xE2\x80\xBA");
    int gap = DEFAULT_THEME.metrics.detail_gap;
    return columns_have_ink(row, right - chevron, right) &&
           !columns_have_ink(row, right - chevron - gap, right - chevron);
}

static void album_rows_show_a_chevron_and_track_rows_do_not(void)
{
    start_app_with_library();
    open_music_entry(MUSIC_ALBUMS_INDEX);
    wait_ms(SLIDE_DURATION_MS);
    CHECK(row_shows_a_chevron(1));
    tap(KEY_PLAY);
    wait_ms(SLIDE_DURATION_MS);
    CHECK(!row_shows_a_chevron(1));
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
    hold(KEY_VOLUME_UP);
    CHECK(playing_path_is(SHOOT_TO_THRILL_PATH));
    CHECK_EQUAL(volume + 1, fake_hal_audio_volume());
    hold(KEY_VOLUME_DOWN);
    CHECK(playing_path_is(HELLS_BELLS_PATH));
}

static void lists_still_move_as_soon_as_a_volume_key_goes_down(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_BACK);
    tap(KEY_BACK);
    CHECK(title_is("Albums"));
    fake_hal_queue_transition(KEY_VOLUME_UP, true, now_ms);
    now_ms += 10;
    step();
    tap(KEY_PLAY);
    CHECK(!title_is("Back In Black"));
}

enum {
    HOME_ART_CYCLE_MS = 5000
};

static uint16_t home_art_probe(void)
{
    return fake_hal_pixel(HAL_DISPLAY_WIDTH - 72, 88);
}

static void home_art_shows_a_library_cover_when_idle(void)
{
    start_app_with_library();
    step();
    step();
    CHECK_EQUAL(RED_COVER, home_art_probe());
}

static void home_art_moves_on_and_skips_albums_without_a_cover(void)
{
    start_app_with_library();
    step();
    wait_ms(HOME_ART_CYCLE_MS);
    step();
    CHECK_EQUAL(BLUE_COVER, home_art_probe());
}

static void home_art_keeps_the_playing_album(void)
{
    start_app_with_library();
    play_first_album_track();
    hold_back();
    wait_ms(SLIDE_DURATION_MS);
    for (int cycle = 0; cycle < 3; cycle++) {
        wait_ms(HOME_ART_CYCLE_MS);
    }
    step();
    CHECK(title_is("EchoPod"));
    CHECK_EQUAL(RED_COVER, home_art_probe());
}

static void the_home_menu_shows_now_playing_last_once_a_track_is_loaded(void)
{
    start_app_with_library();
    play_first_album_track();
    hold_back();
    CHECK(title_is("EchoPod"));
    move_down(HOME_NOW_PLAYING_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Now Playing"));
}

static void the_home_menu_hides_now_playing_with_nothing_loaded(void)
{
    start_app_with_library();
    move_down(HOME_NOW_PLAYING_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Music"));
}

static void open_setting(int index)
{
    move_down(HOME_SETTINGS_INDEX);
    tap(KEY_PLAY);
    move_down(index);
    tap(KEY_PLAY);
}

static void settings_choose_shuffle_and_repeat(void)
{
    start_app_with_library();
    open_setting(0);
    CHECK(title_is("Shuffle"));
    move_down(1);
    tap(KEY_PLAY);
    CHECK(title_is("Settings"));
    CHECK_EQUAL(SHUFFLE_SONGS, app_player_status().shuffle);
    move_down(SETTINGS_REPEAT_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Repeat"));
    move_down(2);
    tap(KEY_PLAY);
    CHECK_EQUAL(REPEAT_ALL, app_player_status().repeat);
}

static bool saved_settings(Settings *settings)
{
    uint32_t size = 0;
    const uint8_t *bytes = fake_hal_file_bytes(SETTINGS_STORE_PATH, &size);
    return bytes != NULL && settings_decode(bytes, size, settings);
}

static void brightness_is_applied_and_saved(void)
{
    start_app_with_library();
    CHECK_EQUAL(settings_defaults().brightness, fake_hal_display_brightness());
    open_setting(SETTINGS_BRIGHTNESS_INDEX);
    CHECK(title_is("Brightness"));
    move_down(1);
    tap(KEY_PLAY);
    CHECK_EQUAL(settings_defaults().brightness + 1, fake_hal_display_brightness());
    Settings settings;
    CHECK(saved_settings(&settings));
    CHECK_EQUAL(settings_defaults().brightness + 1, settings.brightness);
}

static void gain_is_applied(void)
{
    start_app_with_library();
    open_setting(SETTINGS_GAIN_INDEX);
    move_down(1);
    tap(KEY_PLAY);
    CHECK_EQUAL(HAL_AUDIO_GAIN_HIGH, fake_hal_audio_gain());
}

static void saved_settings_are_restored_at_start(void)
{
    start_app_with_library();
    open_setting(SETTINGS_BRIGHTNESS_INDEX);
    move_down(2);
    tap(KEY_PLAY);
    restart_app();
    CHECK_EQUAL(settings_defaults().brightness + 2, fake_hal_display_brightness());
}

static uint16_t scrollbar_probe(void)
{
    return fake_hal_pixel(HAL_DISPLAY_WIDTH - DEFAULT_THEME.metrics.scrollbar_inset - 1, 40);
}

static void a_new_screen_slides_in_from_the_right(void)
{
    start_app();
    step();
    tap(KEY_PLAY);
    CHECK(title_is("Music"));
    CHECK(scrollbar_probe() != DEFAULT_THEME.colors.scrollbar_thumb);
    wait_ms(SLIDE_DURATION_MS / 2);
    CHECK(scrollbar_probe() != DEFAULT_THEME.colors.scrollbar_thumb);
    wait_ms(SLIDE_DURATION_MS / 2);
    CHECK_EQUAL(DEFAULT_THEME.colors.scrollbar_thumb, scrollbar_probe());
}

static void going_back_slides_the_screen_out_to_the_right(void)
{
    start_app();
    step();
    tap(KEY_PLAY);
    wait_ms(SLIDE_DURATION_MS);
    tap(KEY_BACK);
    CHECK(title_is("EchoPod"));
    CHECK_EQUAL(DEFAULT_THEME.colors.scrollbar_thumb, scrollbar_probe());
    wait_ms(SLIDE_DURATION_MS);
    CHECK(scrollbar_probe() != DEFAULT_THEME.colors.scrollbar_thumb);
}

static void waking_draws_a_frame_before_the_backlight_comes_on(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_POWER);
    CHECK(!fake_hal_display_on());
    int presents = fake_hal_present_count();
    tap(KEY_POWER);
    CHECK(fake_hal_display_on());
    CHECK(fake_hal_presents_at_power_on() > presents);
}

static void the_clock_never_changes_with_the_screen_while_audio_is_up(void)
{
    start_app_with_library();
    play_first_album_track();
    step();
    CHECK_EQUAL(200, hal_system_clock_mhz());
    tap(KEY_POWER);
    CHECK(!fake_hal_display_on());
    CHECK_EQUAL(200, hal_system_clock_mhz());
    tap(KEY_POWER);
    CHECK_EQUAL(200, hal_system_clock_mhz());
}

static void with_the_screen_off_the_cpu_drops_to_96_mhz_once_audio_is_down(void)
{
    start_app_with_library();
    tap(KEY_POWER);
    CHECK_EQUAL(96, hal_system_clock_mhz());
    tap(KEY_POWER);
    CHECK_EQUAL(200, hal_system_clock_mhz());
    play_first_album_track();
    tap(KEY_POWER);
    CHECK_EQUAL(200, hal_system_clock_mhz());
    fake_hal_audio_set_idle(true);
    step();
    CHECK_EQUAL(96, hal_system_clock_mhz());
    fake_hal_audio_set_idle(false);
    step();
    CHECK_EQUAL(200, hal_system_clock_mhz());
}

static void the_player_turns_itself_off_after_five_idle_minutes_on_battery(void)
{
    start_app_with_library();
    wait_ms(5 * 60000 - 1000);
    CHECK_EQUAL(0, fake_hal_power_off_count());
    wait_ms(1000);
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(1, fake_hal_power_off_count());
}

static void playing_or_usb_power_keeps_the_player_on(void)
{
    start_app_with_library();
    play_first_album_track();
    for (int minute = 0; minute < 12; minute++) {
        wait_ms(60000);
    }
    CHECK_EQUAL(0, fake_hal_power_off_count());
    tap(KEY_PLAY);
    fake_hal_set_battery(
        (HalBattery){.known = true, .percent = 80, .millivolts = 3900, .usb_power = true});
    for (int minute = 0; minute < 12; minute++) {
        wait_ms(60000);
    }
    CHECK_EQUAL(0, fake_hal_power_off_count());
}

static void the_screen_turns_off_after_the_timeout(void)
{
    start_app();
    step();
    CHECK(fake_hal_display_on());
    wait_ms(DEFAULT_TIMEOUT_MS);
    CHECK(!fake_hal_display_on());
    int presents = fake_hal_present_count();
    step();
    CHECK_EQUAL(presents, fake_hal_present_count());
}

static void after_a_timeout_volume_changes_without_waking(void)
{
    start_app_with_library();
    play_first_album_track();
    uint8_t volume = app_player_status().volume;
    wait_ms(DEFAULT_TIMEOUT_MS);
    CHECK(!fake_hal_display_on());
    tap(KEY_VOLUME_UP);
    CHECK_EQUAL(volume + 1, app_player_status().volume);
    CHECK(!fake_hal_display_on());
}

static void the_timeout_setting_is_used(void)
{
    start_app();
    open_setting(SETTINGS_TIMEOUT_INDEX);
    CHECK(title_is("Screen timeout"));
    move_up(2);
    tap(KEY_PLAY);
    wait_ms(SHORT_TIMEOUT_MS);
    CHECK(!fake_hal_display_on());
}

static void with_the_screen_off_media_keys_keep_working(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_BACK);
    CHECK(!title_is("Now Playing"));
    tap(KEY_POWER);
    CHECK(!fake_hal_display_on());
    uint8_t volume = app_player_status().volume;
    tap(KEY_VOLUME_UP);
    CHECK_EQUAL(volume + 1, app_player_status().volume);
    tap(KEY_PLAY);
    CHECK_EQUAL(PLAYER_PAUSED, app_player_status().state);
    tap(KEY_PLAY);
    CHECK_EQUAL(PLAYER_PLAYING, app_player_status().state);
    hold(KEY_VOLUME_UP);
    CHECK(playing_path_is(SHOOT_TO_THRILL_PATH));
    const char *title = app_screen_title();
    tap(KEY_MENU);
    tap(KEY_BACK);
    CHECK(title_is(title));
    CHECK(!fake_hal_display_on());
    tap(KEY_POWER);
    CHECK(fake_hal_display_on());
}

static void holding_power_saves_and_turns_the_player_off(void)
{
    start_app_with_library();
    play_first_album_track();
    int writes = fake_hal_write_count();
    hold_power();
    CHECK(fake_hal_write_count() > writes);
    CHECK_EQUAL(0, fake_hal_power_off_count());
    CHECK(fake_hal_display_on());
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(1, fake_hal_power_off_count());
    CHECK(!fake_hal_display_on());
    tap(KEY_PLAY);
    CHECK_EQUAL(1, fake_hal_power_off_count());
}

static void a_one_second_power_hold_only_turns_the_screen_off(void)
{
    start_app_with_library();
    play_first_album_track();
    hold_for(KEY_POWER, 1000);
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(0, fake_hal_power_off_count());
    CHECK(!fake_hal_display_on());
}

static void a_slightly_long_press_wakes_the_screen_without_switching_off(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_POWER);
    CHECK(!fake_hal_display_on());
    hold_for(KEY_POWER, 1000);
    CHECK_EQUAL(0, fake_hal_power_off_count());
    CHECK(fake_hal_display_on());
}

static void a_volume_hold_under_a_second_changes_the_volume(void)
{
    start_app_with_library();
    play_first_album_track();
    uint8_t volume = fake_hal_audio_volume();
    hold_for(KEY_VOLUME_UP, 800);
    CHECK_EQUAL(volume + 1, fake_hal_audio_volume());
    CHECK(playing_path_is(HELLS_BELLS_PATH));
}

static void keys_are_ignored_while_saying_goodbye(void)
{
    start_app_with_library();
    hold_power();
    const char *title = app_screen_title();
    tap(KEY_PLAY);
    CHECK(title_is(title));
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(1, fake_hal_power_off_count());
}

static void the_splash_ignores_keys_until_it_ends(void)
{
    start_app();
    app_show_splash();
    step();
    tap(KEY_PLAY);
    CHECK(title_is("EchoPod"));
    wait_ms(SPLASH_MS);
    tap(KEY_PLAY);
    CHECK(title_is("Music"));
}

static void the_splash_draws_over_the_home_screen(void)
{
    start_app();
    app_show_splash();
    wait_ms(SPLASH_FADE_MS);
    Rect content = theme_content_area(&DEFAULT_THEME);
    CHECK_EQUAL(DEFAULT_THEME.colors.background, fake_hal_pixel(content.x + 2, content.y + 4));
}

static bool toast_is(const char *expected)
{
    return app_toast_text() != NULL && strcmp(expected, app_toast_text()) == 0;
}

static void a_refused_track_says_why_it_was_skipped(void)
{
    start_app_with_library();
    fake_hal_audio_set_refusal(
        (HalAudioRefusal){.problem = HAL_AUDIO_PROBLEM_RATE, .sample_rate = 192000});
    fake_hal_audio_refuse_plays(1);
    play_first_album_track();
    CHECK(playing_path_is(SHOOT_TO_THRILL_PATH));
    CHECK(toast_is("192 kHz isn't supported yet"));
}

static void rescanning_without_a_card_says_so(void)
{
    start_app_with_library();
    open_setting(SETTINGS_RESCAN_INDEX);
    step();
    CHECK_EQUAL(0, fake_hal_scratch_borrow_count());
    CHECK(toast_is("No music found on the card"));
}

static void a_rescan_of_an_empty_card_writes_an_empty_library(void)
{
    start_app_with_library();
    fake_hal_set_empty_card(true);
    app_request_rescan();
    CHECK_EQUAL(0, fake_hal_scratch_borrow_count());
    step();
    CHECK_EQUAL(1, fake_hal_scratch_borrow_count());
    CHECK_EQUAL(0, fake_hal_scratch_outstanding());
    CHECK(fake_hal_library_writes() > 0);
    CHECK(toast_is("Library: 0 tracks"));
}

static void start_on_a_fresh_card(HalReserveResult result)
{
    fake_hal_reset();
    fake_hal_set_empty_card(true);
    fake_hal_set_reserve_result(result);
    now_ms = START_MS;
    fake_hal_set_milliseconds(now_ms);
    app_start();
}

static void a_fresh_card_gets_the_player_files_and_a_first_scan(void)
{
    start_on_a_fresh_card(HAL_RESERVE_CREATED);
    CHECK_EQUAL(4096, fake_hal_reserved_size("/.echo/settings.bin"));
    CHECK_EQUAL(4096, fake_hal_reserved_size("/.echo/resume.bin"));
    CHECK_EQUAL(4096, fake_hal_reserved_size("/.echo/favorites.bin"));
    CHECK_EQUAL(4 * 1024 * 1024, fake_hal_reserved_size("/.echo/library.db"));
    uint32_t covers_size = fake_hal_reserved_size("/.echo/covers.bin");
    CHECK_EQUAL(80 * 1024 * 1024, covers_size);
    CHECK((covers_size - COVERS_HEADER_SIZE) / (COVERS_ENTRY_SIZE + COVER_BUILD_RECORD_BYTES) >=
          2500);
    CHECK_EQUAL(4096, fake_hal_reserved_size("/.echo/scan.bin"));
    CHECK_EQUAL(4096, fake_hal_reserved_size("/.echo/upnext.bin"));
    CHECK_EQUAL(4096, fake_hal_reserved_size("/.echo/bluetooth.bin"));
    step();
    CHECK_EQUAL(1, fake_hal_scratch_borrow_count());
    CHECK(toast_is("Library: 0 tracks"));
}

static void a_card_that_has_the_files_is_not_rescanned(void)
{
    start_on_a_fresh_card(HAL_RESERVE_EXISTS);
    CHECK_EQUAL(11, fake_hal_reserve_count());
    step();
    CHECK_EQUAL(0, fake_hal_scratch_borrow_count());
}

static void unchanged_music_is_not_rescanned_after_usb(void)
{
    start_on_a_fresh_card(HAL_RESERVE_CREATED);
    step();
    CHECK_EQUAL(1, fake_hal_scratch_borrow_count());
    app_request_rescan_if_music_changed();
    step();
    CHECK_EQUAL(1, fake_hal_scratch_borrow_count());
}

static void a_new_firmware_checks_the_music_once_without_rescanning(void)
{
    start_on_a_fresh_card(HAL_RESERVE_CREATED);
    step();
    CHECK_EQUAL(1, fake_hal_scratch_borrow_count());
    fake_hal_set_version("newer");
    int walks_before = fake_hal_dir_open_count();
    restart_app();
    step();
    CHECK(fake_hal_dir_open_count() > walks_before);
    CHECK_EQUAL(1, fake_hal_scratch_borrow_count());
    int walks_after = fake_hal_dir_open_count();
    restart_app();
    step();
    CHECK_EQUAL(walks_after, fake_hal_dir_open_count());
}

static void the_music_check_after_usb_says_so_before_walking_the_card(void)
{
    start_on_a_fresh_card(HAL_RESERVE_CREATED);
    step();
    int presents = fake_hal_present_count();
    app_request_rescan_if_music_changed();
    step();
    CHECK(fake_hal_presents_at_last_dir_open() > presents);
}

static void a_new_firmware_rescans_music_that_changed(void)
{
    start_on_a_fresh_card(HAL_RESERVE_CREATED);
    step();
    static const uint8_t BLANK[16] = {0};
    CHECK(hal_file_write_at("/.echo/scan.bin", 0, BLANK, sizeof BLANK));
    fake_hal_set_version("newer");
    restart_app();
    step();
    CHECK_EQUAL(2, fake_hal_scratch_borrow_count());
}

static void a_new_firmware_rebuilds_a_library_it_cannot_read(void)
{
    start_on_a_fresh_card(HAL_RESERVE_CREATED);
    step();
    static const uint8_t OLD_VERSION[2] = {2, 0};
    CHECK(hal_file_write_at("/.echo/library.db", 4, OLD_VERSION, sizeof OLD_VERSION));
    fake_hal_set_version("newer");
    restart_app();
    step();
    CHECK_EQUAL(2, fake_hal_scratch_borrow_count());
}

static void a_different_stored_fingerprint_is_rescanned_after_usb(void)
{
    start_on_a_fresh_card(HAL_RESERVE_CREATED);
    step();
    static const uint8_t BLANK[16] = {0};
    CHECK(hal_file_write_at("/.echo/scan.bin", 0, BLANK, sizeof BLANK));
    app_request_rescan_if_music_changed();
    step();
    CHECK_EQUAL(2, fake_hal_scratch_borrow_count());
}

static void a_card_that_cannot_be_walked_is_rescanned_after_usb(void)
{
    start_on_a_fresh_card(HAL_RESERVE_CREATED);
    step();
    fake_hal_set_empty_card(false);
    app_request_rescan_if_music_changed();
    step();
    CHECK(toast_is("No music found on the card"));
}

static void a_full_card_says_it_is_full(void)
{
    start_on_a_fresh_card(HAL_RESERVE_NO_SPACE);
    step();
    CHECK_EQUAL(0, fake_hal_scratch_borrow_count());
    CHECK(toast_is("The card is full"));
}

static void a_rescan_after_an_installed_update_asks_for_a_restart(void)
{
    start_app_with_library();
    fake_hal_set_empty_card(true);
    fake_hal_set_restart_pending(true);
    app_request_rescan();
    step();
    CHECK_EQUAL(0, fake_hal_scratch_borrow_count());
    CHECK_EQUAL(0, fake_hal_library_writes());
    CHECK(toast_is("Restart the player to finish the update"));
}

static void the_music_check_after_an_installed_update_skips_the_walk(void)
{
    start_app_with_library();
    fake_hal_set_empty_card(true);
    fake_hal_set_restart_pending(true);
    int before = fake_hal_dir_open_count();
    app_request_rescan_if_music_changed();
    step();
    CHECK_EQUAL(before, fake_hal_dir_open_count());
    CHECK(toast_is("Restart the player to finish the update"));
}

static void a_rescan_without_scratch_memory_says_so(void)
{
    start_app_with_library();
    fake_hal_set_empty_card(true);
    fake_hal_refuse_scratch(true);
    app_request_rescan();
    step();
    fake_hal_refuse_scratch(false);
    CHECK(toast_is("Couldn't free memory for the scan"));
}

static bool rescan_message_is(ScanResult result, const char *expected)
{
    char text[48];
    RescanOutcome outcome = {.result = result};
    return strcmp(expected, library_rescan_message(outcome, text, sizeof text)) == 0;
}

static void each_way_a_scan_runs_out_has_its_own_message(void)
{
    CHECK(rescan_message_is(SCAN_OUT_OF_MEMORY, "Library too big for the player's memory"));
    CHECK(rescan_message_is(SCAN_TOO_MANY, "Too many tracks for the player"));
}

static void a_rescan_borrows_the_idle_audio_buffer_for_its_listing(void)
{
    start_app_with_library();
    fake_hal_set_empty_card(true);
    app_request_rescan();
    step();
    CHECK_EQUAL(1, fake_hal_listing_borrow_count());
}

static void a_rescan_rebuilds_the_covers_file(void)
{
    start_app_with_library();
    fake_hal_set_empty_card(true);
    app_request_rescan();
    step();
    CHECK(fake_hal_covers_writes() > 0);
    CHECK(toast_is("Library: 0 tracks"));
}

static void a_rescan_keeps_the_stall_watch_fed(void)
{
    start_app_with_library();
    fake_hal_set_empty_card(true);
    app_request_rescan();
    int before = fake_hal_keep_alive_count();
    step();
    CHECK(fake_hal_keep_alive_count() > before);
}

static void the_sleep_timer_turns_the_player_off(void)
{
    start_app_with_library();
    fake_hal_set_battery(
        (HalBattery){.known = true, .percent = 80, .millivolts = 3900, .usb_power = true});
    open_setting(SETTINGS_SLEEP_INDEX);
    CHECK(title_is("Sleep timer"));
    move_down(1);
    tap(KEY_PLAY);
    CHECK(app_toast_text() != NULL);
    wait_ms(FIFTEEN_MINUTES_MS - 1000);
    CHECK(!app_charging());
    wait_ms(1000);
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(0, fake_hal_power_off_count());
    CHECK(app_charging());
}

static const HalBattery ON_USB = {
    .known = true, .percent = 64, .millivolts = 3900, .usb_power = true, .charging = true};

static void switching_off_on_usb_power_shows_the_charge_screen(void)
{
    start_app_with_library();
    play_first_album_track();
    fake_hal_set_battery(ON_USB);
    hold_power();
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(0, fake_hal_power_off_count());
    CHECK(app_charging());
    CHECK(fake_hal_display_on());
    CHECK_EQUAL(PLAYER_PAUSED, app_player_status().state);
}

static void the_charge_screen_goes_dark_and_a_key_lights_it(void)
{
    start_app_with_library();
    fake_hal_set_battery(ON_USB);
    hold_power();
    wait_ms(GOODBYE_MS);
    wait_ms(5000);
    CHECK(!fake_hal_display_on());
    tap(KEY_PLAY);
    CHECK(fake_hal_display_on());
    CHECK(app_charging());
}

static void holding_power_on_the_charge_screen_turns_the_player_on(void)
{
    start_app_with_library();
    fake_hal_set_battery(ON_USB);
    hold_power();
    wait_ms(GOODBYE_MS);
    hold_power();
    CHECK(!app_charging());
    CHECK(fake_hal_display_on());
    CHECK_EQUAL(0, fake_hal_power_off_count());
}

static void unplugging_on_the_charge_screen_turns_the_player_off(void)
{
    start_app_with_library();
    fake_hal_set_battery(ON_USB);
    hold_power();
    wait_ms(GOODBYE_MS);
    fake_hal_set_battery((HalBattery){.known = true, .percent = 64, .millivolts = 3900});
    wait_ms(500);
    CHECK_EQUAL(1, fake_hal_power_off_count());
}

static void a_cable_start_opens_the_charge_screen(void)
{
    fake_hal_reset();
    fake_hal_set_battery(ON_USB);
    fake_hal_set_usb_start(true);
    now_ms = START_MS;
    fake_hal_set_milliseconds(now_ms);
    app_start();
    CHECK(app_charging());
    wait_ms(100);
    CHECK(fake_hal_display_on());
    CHECK(!fake_hal_bluetooth_powered());
}

static void an_empty_battery_saves_and_turns_the_player_off(void)
{
    start_app_with_library();
    play_first_album_track();
    int writes = fake_hal_write_count();
    fake_hal_set_battery((HalBattery){.known = true, .percent = 0, .millivolts = 3400});
    wait_ms(300);
    wait_ms(1900);
    CHECK_EQUAL(0, fake_hal_power_off_count());
    wait_ms(300);
    CHECK(fake_hal_write_count() > writes);
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(1, fake_hal_power_off_count());
}

static void an_empty_battery_on_usb_power_keeps_playing(void)
{
    start_app_with_library();
    play_first_album_track();
    fake_hal_set_battery(
        (HalBattery){.known = true, .percent = 0, .millivolts = 3400, .usb_power = true});
    wait_ms(5000);
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(0, fake_hal_power_off_count());
}

static void open_albums_and_hold_the_first(void)
{
    open_music_entry(MUSIC_ALBUMS_INDEX);
    hold(KEY_PLAY);
}

static void open_first_album(void)
{
    open_music_entry(MUSIC_ALBUMS_INDEX);
    tap(KEY_PLAY);
}

static void holding_play_on_a_song_offers_up_next(void)
{
    start_app_with_library();
    open_first_album();
    hold(KEY_PLAY);
    CHECK(title_is("Hells Bells"));
    tap(KEY_BACK);
    CHECK(title_is("Back In Black"));
}

static void play_next_from_a_list_queues_the_song(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_BACK);
    move_down(1);
    hold(KEY_PLAY);
    tap(KEY_PLAY);
    CHECK(toast_is("Playing next"));
    CHECK(title_is("Back In Black"));
    CHECK(playing_path_is(HELLS_BELLS_PATH));
}

static void add_to_up_next_from_a_list_queues_the_song(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_BACK);
    move_down(1);
    hold(KEY_PLAY);
    move_down(1);
    tap(KEY_PLAY);
    CHECK(toast_is("Added to Up Next"));
    CHECK(title_is("Back In Black"));
}

static void play_next_with_nothing_playing_starts_the_song(void)
{
    start_app_with_library();
    open_first_album();
    hold(KEY_PLAY);
    tap(KEY_PLAY);
    CHECK(playing_path_is(HELLS_BELLS_PATH));
    CHECK(toast_is("Playing"));
}

static void menu_on_now_playing_opens_up_next(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_MENU);
    CHECK(title_is("Up Next"));
    tap(KEY_MENU);
    CHECK(title_is("Now Playing"));
    tap(KEY_MENU);
    tap(KEY_BACK);
    CHECK(title_is("Now Playing"));
}

static void choosing_a_song_in_up_next_plays_it(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_MENU);
    move_down(1);
    tap(KEY_PLAY);
    CHECK(title_is("Now Playing"));
    CHECK(playing_path_is(SHOOT_TO_THRILL_PATH));
}

static void holding_play_on_an_album_adds_it_to_favorites(void)
{
    start_app_with_library();
    open_albums_and_hold_the_first();
    CHECK(title_is("Albums"));
    CHECK(toast_is("Added to Favorites"));
    tap(KEY_BACK);
    move_down(MUSIC_FAVORITES_INDEX - MUSIC_ALBUMS_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Favorites"));
    tap(KEY_PLAY);
    CHECK(title_is("Back In Black"));
}

static void holding_play_in_favorites_removes_the_album(void)
{
    start_app_with_library();
    open_albums_and_hold_the_first();
    tap(KEY_BACK);
    move_down(MUSIC_FAVORITES_INDEX - MUSIC_ALBUMS_INDEX);
    tap(KEY_PLAY);
    hold(KEY_PLAY);
    CHECK(toast_is("Removed from Favorites"));
    tap(KEY_PLAY);
    CHECK(title_is("Favorites"));
}

static void favorites_survive_a_restart(void)
{
    start_app_with_library();
    open_albums_and_hold_the_first();
    restart_app();
    open_music_entry(MUSIC_FAVORITES_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Back In Black"));
}

static void a_toast_disappears_after_a_while(void)
{
    start_app_with_library();
    open_albums_and_hold_the_first();
    wait_ms(TOAST_DURATION_MS);
    CHECK(app_toast_text() == NULL);
}

static void playlists_open_from_the_music_menu(void)
{
    start_app_with_library();
    open_music_entry(MUSIC_PLAYLISTS_INDEX);
    CHECK(title_is("Playlists"));
    tap(KEY_PLAY);
    CHECK(title_is("Road Trip"));
    tap(KEY_PLAY);
    CHECK(playing_path_is(SHOOTS_PATH));
}

static void start_with(StartScreen start)
{
    start_app_with_library();
    open_setting(SETTINGS_START_INDEX);
    CHECK(title_is("Start screen"));
    move_down((int)start);
    tap(KEY_PLAY);
    restart_app();
}

static void the_start_screen_setting_opens_albums(void)
{
    start_with(START_ALBUMS);
    CHECK(title_is("Albums"));
    tap(KEY_BACK);
    CHECK(title_is("Music"));
    tap(KEY_BACK);
    CHECK(title_is("EchoPod"));
}

static void the_start_screen_setting_opens_now_playing(void)
{
    start_with(START_NOW_PLAYING);
    CHECK(title_is("Now Playing"));
    CHECK_EQUAL(2, app_screen_depth());
}

static void settle(void)
{
    for (int count = 0; count < SETTLE_STEPS; count++) {
        wait_ms(16);
    }
}

static void cover_flow_opens_from_the_music_menu(void)
{
    start_app_with_library();
    open_music_entry(MUSIC_COVER_FLOW_INDEX);
    CHECK(title_is("Cover Flow"));
    tap(KEY_BACK);
    CHECK(title_is("Music"));
}

static void cover_flow_needs_a_library(void)
{
    start_app();
    open_music_entry(MUSIC_COVER_FLOW_INDEX);
    CHECK(title_is("No library"));
}

static void cover_flow_runs_by_album_artist_and_opens_the_centre_album(void)
{
    start_app_with_library();
    open_music_entry(MUSIC_COVER_FLOW_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("High Voltage"));
    tap(KEY_BACK);
    move_down(1);
    tap(KEY_PLAY);
    CHECK(title_is("Back In Black"));
}

static void cover_flow_shows_the_centre_cover(void)
{
    start_app_with_library();
    open_music_entry(MUSIC_COVER_FLOW_INDEX);
    settle();
    CHECK_EQUAL(BLUE_COVER, fake_hal_pixel(COVER_CENTER_X, COVER_CENTER_Y));
    move_down(1);
    settle();
    CHECK_EQUAL(RED_COVER, fake_hal_pixel(COVER_CENTER_X, COVER_CENTER_Y));
}

static void cover_flow_starts_at_the_playing_album(void)
{
    start_app_with_library();
    play_first_album_track();
    hold_back();
    open_music_entry(MUSIC_COVER_FLOW_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Back In Black"));
}

static void holding_play_in_cover_flow_toggles_a_favorite(void)
{
    start_app_with_library();
    open_music_entry(MUSIC_COVER_FLOW_INDEX);
    hold(KEY_PLAY);
    CHECK(toast_is("Added to Favorites"));
    CHECK(title_is("Cover Flow"));
}

static bool area_has_color(Rect area, Color color)
{
    for (int y = area.y; y < area.y + area.height; y++) {
        for (int x = area.x; x < area.x + area.width; x++) {
            if (fake_hal_pixel(x, y) == color) {
                return true;
            }
        }
    }
    return false;
}

static void the_header_shows_the_play_state_and_battery(void)
{
    start_app_with_library();
    fake_hal_set_battery((HalBattery){.known = true, .percent = HALF_BATTERY, .charging = false});
    Color icon = DEFAULT_THEME.colors.header_text;
    step();
    CHECK(!area_has_color(status_bar_playback_area(&DEFAULT_THEME), icon));
    CHECK(area_has_color(status_bar_battery_area(&DEFAULT_THEME), icon));
    play_first_album_track();
    CHECK(area_has_color(status_bar_playback_area(&DEFAULT_THEME), icon));
}

static void stopping_the_app_saves_the_session(void)
{
    start_app_with_library();
    play_first_album_track();
    int writes = fake_hal_write_count();
    app_stop();
    Settings settings;
    CHECK_EQUAL(writes + 2, fake_hal_write_count());
    CHECK(saved_settings(&settings));
}

static void each_step_records_its_duration(void)
{
    start_app();
    fake_hal_set_microseconds_per_read(4000);
    uint32_t before_us = fake_hal_microseconds();
    step();
    uint32_t first_read_us = before_us + 4000;
    CHECK_EQUAL(fake_hal_microseconds() - first_read_us, frame_stats_latest_us(app_frame_stats()));
    CHECK(frame_stats_latest_us(app_frame_stats()) > 0);
}

static void a_notice_is_drawn_at_once_over_the_current_screen(void)
{
    start_app();
    step();
    int presents = fake_hal_present_count();
    long pixels = fake_hal_pixels_written();
    app_show_notice("USB", "Connected to a computer");
    CHECK_EQUAL(presents + 1, fake_hal_present_count());
    CHECK(fake_hal_pixels_written() > pixels);
    CHECK(title_is("EchoPod"));
}

static void a_notice_turns_the_display_on(void)
{
    start_app();
    wait_ms(DEFAULT_TIMEOUT_MS);
    CHECK(!fake_hal_display_on());
    app_show_notice("USB", "Connected to a computer");
    CHECK(fake_hal_display_on());
}

static void waking_keeps_the_screen_on_for_a_full_timeout(void)
{
    start_app();
    step();
    now_ms += DEFAULT_TIMEOUT_MS * 4;
    fake_hal_set_milliseconds(now_ms);
    app_wake();
    wait_ms(DEFAULT_TIMEOUT_MS - 1000);
    CHECK(fake_hal_display_on());
    wait_ms(1000);
    CHECK(!fake_hal_display_on());
}

static void diagnostics_shows_the_firmware_version_from_the_hal(void)
{
    start_app();
    fake_hal_set_version("1e8bebb");
    const char *value = app_diagnostics_value(DIAGNOSTICS_FIRMWARE_INDEX);
    CHECK(value != NULL);
    CHECK(strcmp("1e8bebb", value) == 0);
}

static uint16_t cover_probe(void)
{
    return fake_hal_pixel(DEFAULT_THEME.metrics.padding + 56, 108);
}

static void now_playing_shows_the_album_cover(void)
{
    start_app_with_library();
    play_first_album_track();
    wait_ms(SLIDE_DURATION_MS);
    step();
    CHECK_EQUAL(0xF800, cover_probe());
}

static void now_playing_without_a_cover_keeps_the_text_layout(void)
{
    start_app_with_library();
    open_music_entry(MUSIC_ALBUMS_INDEX);
    move_down(1);
    tap(KEY_PLAY);
    tap(KEY_PLAY);
    wait_ms(SLIDE_DURATION_MS);
    step();
    CHECK(title_is("Now Playing"));
    CHECK_EQUAL(DEFAULT_THEME.colors.background, cover_probe());
}

static void an_unchanged_frame_sends_no_pixels(void)
{
    start_app();
    step();
    long pixels = fake_hal_pixels_written();
    step();
    CHECK_EQUAL(pixels, fake_hal_pixels_written());
}

static void moving_the_selection_sends_only_part_of_the_screen(void)
{
    start_app();
    step();
    long pixels = fake_hal_pixels_written();
    tap(KEY_VOLUME_UP);
    long sent = fake_hal_pixels_written() - pixels;
    CHECK(sent > 0);
    CHECK(sent < FULL_FRAME_PIXELS / 2);
}

static void restarting_the_app_sends_a_whole_frame_again(void)
{
    start_app();
    step();
    restart_app();
    long pixels = fake_hal_pixels_written();
    step();
    CHECK_EQUAL(pixels + FULL_FRAME_PIXELS, fake_hal_pixels_written());
}

static void play_on_the_playing_track_opens_now_playing_without_restarting(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_BACK);
    CHECK(!title_is("Now Playing"));
    int plays = fake_hal_audio_play_count();
    tap(KEY_PLAY);
    CHECK(title_is("Now Playing"));
    CHECK_EQUAL(plays, fake_hal_audio_play_count());
}

static void play_on_another_track_starts_it(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_BACK);
    move_down(1);
    int plays = fake_hal_audio_play_count();
    tap(KEY_PLAY);
    CHECK(title_is("Now Playing"));
    CHECK_EQUAL(plays + 1, fake_hal_audio_play_count());
    CHECK(playing_path_is(SHOOT_TO_THRILL_PATH));
}

static void idling_in_the_menus_stays_put_while_playing(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_BACK);
    for (uint32_t waited = 0; waited < IDLE_MS; waited += 1000) {
        wait_ms(1000);
    }
    CHECK(fake_hal_display_on());
    CHECK(!title_is("Now Playing"));
}

static void the_key_test_stays_open_after_waking(void)
{
    start_app_with_library();
    play_first_album_track();
    hold_back();
    move_down(HOME_DIAGNOSTICS_INDEX);
    tap(KEY_PLAY);
    move_down(DIAGNOSTICS_KEY_TEST_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Key test"));
    tap(KEY_POWER);
    tap(KEY_POWER);
    CHECK(title_is("Key test"));
}

static void waking_the_screen_shows_now_playing_while_playing(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_BACK);
    tap(KEY_POWER);
    CHECK(!fake_hal_display_on());
    tap(KEY_POWER);
    CHECK(fake_hal_display_on());
    CHECK(title_is("Now Playing"));
}

static void waking_while_paused_keeps_the_screen(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_PLAY);
    tap(KEY_BACK);
    tap(KEY_POWER);
    tap(KEY_POWER);
    CHECK(!title_is("Now Playing"));
}

static void open_diagnostics_row(int index)
{
    move_down(HOME_DIAGNOSTICS_INDEX);
    tap(KEY_PLAY);
    move_down(index);
}

static void the_cpu_clock_cycles_through_24_96_and_200_mhz(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_CPU_CLOCK_INDEX);
    CHECK_EQUAL(200, hal_system_clock_mhz());
    tap(KEY_PLAY);
    CHECK_EQUAL(24, hal_system_clock_mhz());
    tap(KEY_PLAY);
    CHECK_EQUAL(96, hal_system_clock_mhz());
    tap(KEY_PLAY);
    CHECK_EQUAL(200, hal_system_clock_mhz());
}

static void the_test_tone_plays_once_per_press(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_TEST_TONE_INDEX);
    CHECK_EQUAL(0, fake_hal_test_tone_count());
    tap(KEY_PLAY);
    CHECK_EQUAL(1, fake_hal_test_tone_count());
    CHECK(title_is("Diagnostics"));
}

static void the_stall_test_row_freezes_the_main_loop_on_purpose(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_STALL_TEST_INDEX);
    CHECK_EQUAL(0, fake_hal_stall_test_count());
    tap(KEY_PLAY);
    CHECK_EQUAL(1, fake_hal_stall_test_count());
}

static void the_watchdog_test_row_stops_everything_on_purpose(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_WATCHDOG_TEST_INDEX);
    CHECK_EQUAL(0, fake_hal_watchdog_test_count());
    tap(KEY_PLAY);
    CHECK_EQUAL(1, fake_hal_watchdog_test_count());
}

static void ignored_power_taps_leave_the_screen_on_but_a_hold_still_switches_off(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_POWER_TAPS_INDEX);
    tap(KEY_PLAY);
    tap(KEY_POWER);
    CHECK(fake_hal_display_on());
    hold_power();
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(1, fake_hal_power_off_count());
}

static void the_dma_tone_starts_and_stops_from_one_row(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_DMA_TONE_INDEX);
    CHECK(!hal_audio_dma_tone_status().running);
    tap(KEY_PLAY);
    CHECK(hal_audio_dma_tone_status().running);
    tap(KEY_PLAY);
    CHECK(!hal_audio_dma_tone_status().running);
    CHECK(title_is("Diagnostics"));
}

static void the_high_rate_tone_plays_at_192_khz(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_HIGH_RATE_TONE_INDEX);
    tap(KEY_PLAY);
    CHECK(hal_audio_dma_tone_status().running);
    CHECK_EQUAL(192000u, fake_hal_dma_tone_rate());
    tap(KEY_PLAY);
    CHECK(!hal_audio_dma_tone_status().running);
}

static void the_176_khz_tone_plays_at_176_4_khz(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_TONE_176_INDEX);
    tap(KEY_PLAY);
    CHECK(hal_audio_dma_tone_status().running);
    CHECK_EQUAL(176400u, fake_hal_dma_tone_rate());
    tap(KEY_PLAY);
    CHECK(!hal_audio_dma_tone_status().running);
}

static void the_dma_tone_plays_at_44_1_khz(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_DMA_TONE_INDEX);
    tap(KEY_PLAY);
    CHECK_EQUAL(44100u, fake_hal_dma_tone_rate());
}

static void the_loop_benchmark_runs_once_per_press(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_LOOP_CYCLES_INDEX);
    CHECK_EQUAL(0, fake_hal_loop_bench_count());
    tap(KEY_PLAY);
    CHECK_EQUAL(1, fake_hal_loop_bench_count());
    CHECK(title_is("Diagnostics"));
}

static void the_core_1_test_runs_once_per_press(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_CORE1_INDEX);
    CHECK_EQUAL(0, fake_hal_core1_test_count());
    tap(KEY_PLAY);
    CHECK_EQUAL(1, fake_hal_core1_test_count());
    CHECK(title_is("Diagnostics"));
}

static void the_accelerator_test_runs_once_per_press(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_ACCEL_INDEX);
    CHECK_EQUAL(0, fake_hal_core1_accel_count());
    tap(KEY_PLAY);
    CHECK_EQUAL(1, fake_hal_core1_accel_count());
    CHECK(title_is("Diagnostics"));
}

static void a_clock_that_does_not_lock_stays_where_it_was(void)
{
    start_app();
    fake_hal_refuse_clock_switch();
    open_diagnostics_row(DIAGNOSTICS_CPU_CLOCK_INDEX);
    tap(KEY_PLAY);
    CHECK_EQUAL(200, hal_system_clock_mhz());
    CHECK(title_is("Diagnostics"));
}

static void the_stopwatch_stays_open_after_waking(void)
{
    start_app_with_library();
    play_first_album_track();
    hold_back();
    open_diagnostics_row(DIAGNOSTICS_STOPWATCH_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Stopwatch"));
    tap(KEY_POWER);
    tap(KEY_POWER);
    CHECK(title_is("Stopwatch"));
}

static void menu_opens_now_playing_from_anywhere_while_playing(void)
{
    start_app_with_library();
    play_first_album_track();
    hold_back();
    CHECK(title_is("EchoPod"));
    tap(KEY_MENU);
    CHECK(title_is("Now Playing"));
    int depth = app_screen_depth();
    tap(KEY_MENU);
    CHECK(title_is("Up Next"));
    CHECK_EQUAL(depth + 1, app_screen_depth());
}

static void menu_does_nothing_while_nothing_plays(void)
{
    start_app_with_library();
    tap(KEY_MENU);
    CHECK(title_is("EchoPod"));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_notice_is_drawn_at_once_over_the_current_screen),
        TEST_CASE(a_notice_turns_the_display_on),
        TEST_CASE(waking_keeps_the_screen_on_for_a_full_timeout),
        TEST_CASE(a_step_writes_one_full_frame_and_presents_it),
        TEST_CASE(audio_gets_a_turn_after_every_strip),
        TEST_CASE(bluetooth_gets_a_turn_after_every_strip),
        TEST_CASE(an_unchanged_frame_sends_no_pixels),
        TEST_CASE(moving_the_selection_sends_only_part_of_the_screen),
        TEST_CASE(restarting_the_app_sends_a_whole_frame_again),
        TEST_CASE(the_home_menu_is_the_root),
        TEST_CASE(selecting_music_opens_the_music_menu),
        TEST_CASE(back_returns_to_the_previous_screen),
        TEST_CASE(a_long_back_returns_home_from_deep_screens),
        TEST_CASE(the_frame_overlay_is_off_until_enabled_in_diagnostics),
        TEST_CASE(the_frame_overlay_splits_the_frame_time),
        TEST_CASE(the_frame_overlay_splits_the_drawing),
        TEST_CASE(the_frame_overlay_shows_the_audio_numbers),
        TEST_CASE(the_frame_overlay_shows_average_and_longest_block_times),
        TEST_CASE(the_frame_overlay_shows_zero_before_any_block),
        TEST_CASE(diagnostics_shows_the_firmware_version_from_the_hal),
        TEST_CASE(albums_open_from_the_library),
        TEST_CASE(album_rows_show_a_chevron_and_track_rows_do_not),
        TEST_CASE(an_artist_leads_to_their_albums),
        TEST_CASE(back_returns_to_the_previous_list),
        TEST_CASE(folders_can_be_browsed_down_to_an_album),
        TEST_CASE(without_a_library_the_lists_explain_what_is_missing),
        TEST_CASE(selecting_a_track_plays_it_and_shows_now_playing),
        TEST_CASE(play_on_the_playing_track_opens_now_playing_without_restarting),
        TEST_CASE(play_on_another_track_starts_it),
        TEST_CASE(menu_opens_now_playing_from_anywhere_while_playing),
        TEST_CASE(idling_in_the_menus_stays_put_while_playing),
        TEST_CASE(the_key_test_stays_open_after_waking),
        TEST_CASE(waking_the_screen_shows_now_playing_while_playing),
        TEST_CASE(waking_while_paused_keeps_the_screen),
        TEST_CASE(the_cpu_clock_cycles_through_24_96_and_200_mhz),
        TEST_CASE(the_test_tone_plays_once_per_press),
        TEST_CASE(the_dma_tone_starts_and_stops_from_one_row),
        TEST_CASE(the_high_rate_tone_plays_at_192_khz),
        TEST_CASE(the_176_khz_tone_plays_at_176_4_khz),
        TEST_CASE(the_dma_tone_plays_at_44_1_khz),
        TEST_CASE(the_stall_test_row_freezes_the_main_loop_on_purpose),
        TEST_CASE(the_watchdog_test_row_stops_everything_on_purpose),
        TEST_CASE(ignored_power_taps_leave_the_screen_on_but_a_hold_still_switches_off),
        TEST_CASE(the_loop_benchmark_runs_once_per_press),
        TEST_CASE(the_core_1_test_runs_once_per_press),
        TEST_CASE(the_accelerator_test_runs_once_per_press),
        TEST_CASE(a_clock_that_does_not_lock_stays_where_it_was),
        TEST_CASE(the_stopwatch_stays_open_after_waking),
        TEST_CASE(menu_does_nothing_while_nothing_plays),
        TEST_CASE(now_playing_shows_the_album_cover),
        TEST_CASE(now_playing_without_a_cover_keeps_the_text_layout),
        TEST_CASE(now_playing_keys_control_the_player),
        TEST_CASE(lists_still_move_as_soon_as_a_volume_key_goes_down),
        TEST_CASE(the_home_menu_shows_now_playing_last_once_a_track_is_loaded),
        TEST_CASE(the_home_menu_hides_now_playing_with_nothing_loaded),
        TEST_CASE(home_art_shows_a_library_cover_when_idle),
        TEST_CASE(home_art_moves_on_and_skips_albums_without_a_cover),
        TEST_CASE(home_art_keeps_the_playing_album),
        TEST_CASE(settings_choose_shuffle_and_repeat),
        TEST_CASE(brightness_is_applied_and_saved),
        TEST_CASE(gain_is_applied),
        TEST_CASE(saved_settings_are_restored_at_start),
        TEST_CASE(a_new_screen_slides_in_from_the_right),
        TEST_CASE(going_back_slides_the_screen_out_to_the_right),
        TEST_CASE(waking_draws_a_frame_before_the_backlight_comes_on),
        TEST_CASE(the_clock_never_changes_with_the_screen_while_audio_is_up),
        TEST_CASE(with_the_screen_off_the_cpu_drops_to_96_mhz_once_audio_is_down),
        TEST_CASE(the_player_turns_itself_off_after_five_idle_minutes_on_battery),
        TEST_CASE(playing_or_usb_power_keeps_the_player_on),
        TEST_CASE(the_screen_turns_off_after_the_timeout),
        TEST_CASE(after_a_timeout_volume_changes_without_waking),
        TEST_CASE(the_timeout_setting_is_used),
        TEST_CASE(with_the_screen_off_media_keys_keep_working),
        TEST_CASE(holding_power_saves_and_turns_the_player_off),
        TEST_CASE(a_one_second_power_hold_only_turns_the_screen_off),
        TEST_CASE(a_slightly_long_press_wakes_the_screen_without_switching_off),
        TEST_CASE(a_volume_hold_under_a_second_changes_the_volume),
        TEST_CASE(keys_are_ignored_while_saying_goodbye),
        TEST_CASE(the_splash_ignores_keys_until_it_ends),
        TEST_CASE(the_splash_draws_over_the_home_screen),
        TEST_CASE(the_sleep_timer_turns_the_player_off),
        TEST_CASE(a_refused_track_says_why_it_was_skipped),
        TEST_CASE(rescanning_without_a_card_says_so),
        TEST_CASE(a_rescan_of_an_empty_card_writes_an_empty_library),
        TEST_CASE(a_rescan_keeps_the_stall_watch_fed),
        TEST_CASE(a_rescan_rebuilds_the_covers_file),
        TEST_CASE(a_rescan_after_an_installed_update_asks_for_a_restart),
        TEST_CASE(a_rescan_without_scratch_memory_says_so),
        TEST_CASE(a_rescan_borrows_the_idle_audio_buffer_for_its_listing),
        TEST_CASE(each_way_a_scan_runs_out_has_its_own_message),
        TEST_CASE(the_music_check_after_an_installed_update_skips_the_walk),
        TEST_CASE(a_fresh_card_gets_the_player_files_and_a_first_scan),
        TEST_CASE(a_card_that_has_the_files_is_not_rescanned),
        TEST_CASE(unchanged_music_is_not_rescanned_after_usb),
        TEST_CASE(a_different_stored_fingerprint_is_rescanned_after_usb),
        TEST_CASE(a_new_firmware_checks_the_music_once_without_rescanning),
        TEST_CASE(the_music_check_after_usb_says_so_before_walking_the_card),
        TEST_CASE(a_new_firmware_rescans_music_that_changed),
        TEST_CASE(a_new_firmware_rebuilds_a_library_it_cannot_read),
        TEST_CASE(a_card_that_cannot_be_walked_is_rescanned_after_usb),
        TEST_CASE(a_full_card_says_it_is_full),
        TEST_CASE(an_empty_battery_saves_and_turns_the_player_off),
        TEST_CASE(an_empty_battery_on_usb_power_keeps_playing),
        TEST_CASE(switching_off_on_usb_power_shows_the_charge_screen),
        TEST_CASE(the_charge_screen_goes_dark_and_a_key_lights_it),
        TEST_CASE(holding_power_on_the_charge_screen_turns_the_player_on),
        TEST_CASE(unplugging_on_the_charge_screen_turns_the_player_off),
        TEST_CASE(a_cable_start_opens_the_charge_screen),
        TEST_CASE(holding_play_on_an_album_adds_it_to_favorites),
        TEST_CASE(menu_on_now_playing_opens_up_next),
        TEST_CASE(choosing_a_song_in_up_next_plays_it),
        TEST_CASE(holding_play_on_a_song_offers_up_next),
        TEST_CASE(play_next_from_a_list_queues_the_song),
        TEST_CASE(add_to_up_next_from_a_list_queues_the_song),
        TEST_CASE(play_next_with_nothing_playing_starts_the_song),
        TEST_CASE(holding_play_in_favorites_removes_the_album),
        TEST_CASE(favorites_survive_a_restart),
        TEST_CASE(a_toast_disappears_after_a_while),
        TEST_CASE(playlists_open_from_the_music_menu),
        TEST_CASE(the_start_screen_setting_opens_albums),
        TEST_CASE(the_start_screen_setting_opens_now_playing),
        TEST_CASE(the_header_shows_the_play_state_and_battery),
        TEST_CASE(cover_flow_opens_from_the_music_menu),
        TEST_CASE(cover_flow_needs_a_library),
        TEST_CASE(cover_flow_runs_by_album_artist_and_opens_the_centre_album),
        TEST_CASE(cover_flow_shows_the_centre_cover),
        TEST_CASE(cover_flow_starts_at_the_playing_album),
        TEST_CASE(holding_play_in_cover_flow_toggles_a_favorite),
        TEST_CASE(stopping_the_app_saves_the_session),
        TEST_CASE(each_step_records_its_duration),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

#include <string.h>

#include "check.h"
#include "screens/now_playing_screen.h"
#include "settings/settings.h"

enum {
    TEXT_CAPACITY = 48
};

static const HalAudioFormat HI_RES_FLAC = {.codec = HAL_AUDIO_CODEC_FLAC,
                                           .sample_rate = 96000,
                                           .bits_per_sample = 24,
                                           .channels = 2,
                                           .bitrate_kbps = 2304};
static const HalAudioFormat MP3 = {
    .codec = HAL_AUDIO_CODEC_MP3, .sample_rate = 44100, .channels = 2, .bitrate_kbps = 320};

static bool stream_text_is(HalAudioFormat format, const char *expected)
{
    char buffer[TEXT_CAPACITY];
    now_playing_stream_text(format, buffer, sizeof buffer);
    return strcmp(expected, buffer) == 0;
}

static bool detail_text_is(PlayerStatus status, const char *expected)
{
    char buffer[TEXT_CAPACITY];
    now_playing_detail_text(&status, buffer, sizeof buffer);
    return strcmp(expected, buffer) == 0;
}

static bool modes_text_is(PlayerStatus status, uint8_t speed, const char *expected)
{
    uint8_t options[SETTINGS_OPTION_COUNT] = {0};
    options[OPTION_SPOKEN_SPEED] = speed;
    char buffer[TEXT_CAPACITY];
    now_playing_modes_text(&status, options, buffer, sizeof buffer);
    return strcmp(expected, buffer) == 0;
}

static NowPlayingCommand command(Key key, KeyGesture gesture)
{
    return now_playing_command_for((KeyEvent){.key = key, .gesture = gesture}, false);
}

static NowPlayingCommand scrub_command(Key key, KeyGesture gesture)
{
    return now_playing_command_for((KeyEvent){.key = key, .gesture = gesture}, true);
}

static void a_volume_tap_changes_the_volume(void)
{
    CHECK_EQUAL(NOW_PLAYING_VOLUME_UP, command(KEY_VOLUME_UP, KEY_GESTURE_PRESS));
    CHECK_EQUAL(NOW_PLAYING_VOLUME_DOWN, command(KEY_VOLUME_DOWN, KEY_GESTURE_PRESS));
}

static void holding_volume_up_skips_forward_and_volume_down_skips_back(void)
{
    CHECK_EQUAL(NOW_PLAYING_NEXT, command(KEY_VOLUME_UP, KEY_GESTURE_LONG_PRESS));
    CHECK_EQUAL(NOW_PLAYING_PREVIOUS, command(KEY_VOLUME_DOWN, KEY_GESTURE_LONG_PRESS));
    CHECK_EQUAL(NOW_PLAYING_NONE, command(KEY_VOLUME_UP, KEY_GESTURE_REPEAT));
    CHECK_EQUAL(NOW_PLAYING_NONE, command(KEY_VOLUME_DOWN, KEY_GESTURE_REPEAT));
}

static void play_toggles_pause_and_a_long_play_starts_seeking(void)
{
    CHECK_EQUAL(NOW_PLAYING_TOGGLE_PAUSE, command(KEY_PLAY, KEY_GESTURE_PRESS));
    CHECK_EQUAL(NOW_PLAYING_SCRUB_START, command(KEY_PLAY, KEY_GESTURE_LONG_PRESS));
}

static void while_seeking_volume_keys_move_the_target(void)
{
    CHECK_EQUAL(NOW_PLAYING_SCRUB_FORWARD, scrub_command(KEY_VOLUME_UP, KEY_GESTURE_PRESS));
    CHECK_EQUAL(NOW_PLAYING_SCRUB_FORWARD_FAST,
                scrub_command(KEY_VOLUME_UP, KEY_GESTURE_LONG_PRESS));
    CHECK_EQUAL(NOW_PLAYING_SCRUB_FORWARD_FAST, scrub_command(KEY_VOLUME_UP, KEY_GESTURE_REPEAT));
    CHECK_EQUAL(NOW_PLAYING_SCRUB_BACK, scrub_command(KEY_VOLUME_DOWN, KEY_GESTURE_PRESS));
    CHECK_EQUAL(NOW_PLAYING_SCRUB_BACK_FAST,
                scrub_command(KEY_VOLUME_DOWN, KEY_GESTURE_LONG_PRESS));
    CHECK_EQUAL(NOW_PLAYING_SCRUB_BACK_FAST, scrub_command(KEY_VOLUME_DOWN, KEY_GESTURE_REPEAT));
}

static void while_seeking_play_commits_and_back_cancels(void)
{
    CHECK_EQUAL(NOW_PLAYING_SCRUB_COMMIT, scrub_command(KEY_PLAY, KEY_GESTURE_PRESS));
    CHECK_EQUAL(NOW_PLAYING_SCRUB_CANCEL, scrub_command(KEY_BACK, KEY_GESTURE_PRESS));
    CHECK_EQUAL(NOW_PLAYING_HOME, scrub_command(KEY_BACK, KEY_GESTURE_LONG_PRESS));
    CHECK_EQUAL(NOW_PLAYING_NONE, scrub_command(KEY_MENU, KEY_GESTURE_PRESS));
}

static void the_seek_target_stays_inside_the_track(void)
{
    CHECK_EQUAL(65000, now_playing_scrub_target(60000, 200000, NOW_PLAYING_SCRUB_STEP_MS));
    CHECK_EQUAL(0, now_playing_scrub_target(3000, 200000, -NOW_PLAYING_SCRUB_STEP_MS));
    CHECK_EQUAL(200000, now_playing_scrub_target(198000, 200000, NOW_PLAYING_SCRUB_FAST_STEP_MS));
}

static void menu_opens_up_next_and_holding_it_opens_the_equalizer(void)
{
    CHECK_EQUAL(NOW_PLAYING_UP_NEXT, command(KEY_MENU, KEY_GESTURE_PRESS));
    CHECK_EQUAL(NOW_PLAYING_EQUALIZER, command(KEY_MENU, KEY_GESTURE_LONG_PRESS));
}

static void back_leaves_and_a_long_back_goes_home(void)
{
    CHECK_EQUAL(NOW_PLAYING_LEAVE, command(KEY_BACK, KEY_GESTURE_PRESS));
    CHECK_EQUAL(NOW_PLAYING_HOME, command(KEY_BACK, KEY_GESTURE_LONG_PRESS));
}

static void power_is_left_to_the_system(void)
{
    CHECK_EQUAL(NOW_PLAYING_NONE, command(KEY_POWER, KEY_GESTURE_PRESS));
    CHECK_EQUAL(NOW_PLAYING_NONE, command(KEY_POWER, KEY_GESTURE_LONG_PRESS));
}

static void volume_changes_show_the_volume_bar(void)
{
    CHECK_EQUAL(NOW_PLAYING_FEEDBACK_VOLUME,
                now_playing_feedback_for(NOW_PLAYING_VOLUME_UP, PLAYER_PLAYING));
    CHECK_EQUAL(NOW_PLAYING_FEEDBACK_VOLUME,
                now_playing_feedback_for(NOW_PLAYING_VOLUME_DOWN, PLAYER_PAUSED));
}

static void skips_and_pauses_show_a_badge_for_the_new_state(void)
{
    CHECK_EQUAL(NOW_PLAYING_FEEDBACK_NEXT,
                now_playing_feedback_for(NOW_PLAYING_NEXT, PLAYER_PLAYING));
    CHECK_EQUAL(NOW_PLAYING_FEEDBACK_PREVIOUS,
                now_playing_feedback_for(NOW_PLAYING_PREVIOUS, PLAYER_PLAYING));
    CHECK_EQUAL(NOW_PLAYING_FEEDBACK_PAUSE,
                now_playing_feedback_for(NOW_PLAYING_TOGGLE_PAUSE, PLAYER_PAUSED));
    CHECK_EQUAL(NOW_PLAYING_FEEDBACK_PLAY,
                now_playing_feedback_for(NOW_PLAYING_TOGGLE_PAUSE, PLAYER_PLAYING));
    CHECK_EQUAL(NOW_PLAYING_FEEDBACK_NONE,
                now_playing_feedback_for(NOW_PLAYING_LEAVE, PLAYER_PLAYING));
    CHECK_EQUAL(NOW_PLAYING_FEEDBACK_NONE,
                now_playing_feedback_for(NOW_PLAYING_SCRUB_START, PLAYER_PLAYING));
}

static void the_volume_bar_stays_longer_than_a_badge(void)
{
    CHECK(now_playing_feedback_visible(NOW_PLAYING_FEEDBACK_VOLUME, 1000, 2499));
    CHECK(!now_playing_feedback_visible(NOW_PLAYING_FEEDBACK_VOLUME, 1000, 2500));
    CHECK(now_playing_feedback_visible(NOW_PLAYING_FEEDBACK_NEXT, 1000, 1699));
    CHECK(!now_playing_feedback_visible(NOW_PLAYING_FEEDBACK_NEXT, 1000, 1700));
    CHECK(!now_playing_feedback_visible(NOW_PLAYING_FEEDBACK_NONE, 1000, 1000));
}

static void the_volume_bar_fills_in_proportion_to_the_level(void)
{
    CHECK_EQUAL(0, now_playing_volume_width(0, 100));
    CHECK_EQUAL(50, now_playing_volume_width(25, 100));
    CHECK_EQUAL(100, now_playing_volume_width(50, 100));
    CHECK_EQUAL(100, now_playing_volume_width(80, 100));
}

static void progress_fills_in_proportion_to_the_position(void)
{
    CHECK_EQUAL(0, now_playing_progress_width(0, 200000, 300));
    CHECK_EQUAL(150, now_playing_progress_width(100000, 200000, 300));
    CHECK_EQUAL(300, now_playing_progress_width(200000, 200000, 300));
}

static void progress_stays_inside_the_bar(void)
{
    CHECK_EQUAL(300, now_playing_progress_width(250000, 200000, 300));
    CHECK_EQUAL(0, now_playing_progress_width(5000, 0, 300));
}

static void the_codec_has_a_short_name(void)
{
    CHECK(strcmp("FLAC", now_playing_codec_name(HAL_AUDIO_CODEC_FLAC)) == 0);
    CHECK(strcmp("MP3", now_playing_codec_name(HAL_AUDIO_CODEC_MP3)) == 0);
    CHECK(strcmp("WAV", now_playing_codec_name(HAL_AUDIO_CODEC_WAV)) == 0);
    CHECK(strcmp("ALAC", now_playing_codec_name(HAL_AUDIO_CODEC_ALAC)) == 0);
    CHECK(strcmp("AAC", now_playing_codec_name(HAL_AUDIO_CODEC_AAC)) == 0);
    CHECK(strcmp("Opus", now_playing_codec_name(HAL_AUDIO_CODEC_OPUS)) == 0);
    CHECK(strcmp("QOA", now_playing_codec_name(HAL_AUDIO_CODEC_QOA)) == 0);
    CHECK(strcmp("", now_playing_codec_name(HAL_AUDIO_CODEC_UNKNOWN)) == 0);
}

static void lossless_streams_show_bit_depth_and_rate(void)
{
    CHECK(stream_text_is(HI_RES_FLAC, "24-bit \xC2\xB7 96 kHz"));
}

static void lossy_streams_show_only_the_rate(void)
{
    CHECK(stream_text_is(MP3, "44.1 kHz"));
}

static void an_unknown_stream_shows_nothing(void)
{
    CHECK(stream_text_is((HalAudioFormat){.codec = HAL_AUDIO_CODEC_UNKNOWN}, ""));
}

static void the_detail_line_shows_bitrate_and_place(void)
{
    PlayerStatus status = {.format = HI_RES_FLAC, .place = {3, 10}};
    CHECK(detail_text_is(status, "2,304 kbps \xC2\xB7 3 of 10"));
}

static void spoken_word_shows_its_speed_on_the_modes_line(void)
{
    CHECK(modes_text_is((PlayerStatus){.spoken = true}, SPOKEN_SPEED_1_5X, "1.5x"));
}

static void the_speed_comes_before_shuffle_and_repeat(void)
{
    PlayerStatus status = {.spoken = true, .repeat = REPEAT_ALL};
    CHECK(modes_text_is(status, SPOKEN_SPEED_1_5X, "1.5x \xC2\xB7 Repeat All"));
}

static void every_speed_has_a_short_label(void)
{
    PlayerStatus status = {.spoken = true};
    CHECK(modes_text_is(status, SPOKEN_SPEED_0_75X, "0.75x"));
    CHECK(modes_text_is(status, SPOKEN_SPEED_1_25X, "1.25x"));
    CHECK(modes_text_is(status, SPOKEN_SPEED_1_75X, "1.75x"));
    CHECK(modes_text_is(status, SPOKEN_SPEED_2X, "2x"));
}

static void music_and_normal_speed_show_no_speed(void)
{
    char buffer[TEXT_CAPACITY];
    PlayerStatus book = {.spoken = true};
    CHECK(modes_text_is((PlayerStatus){0}, SPOKEN_SPEED_2X, ""));
    CHECK(modes_text_is(book, SPOKEN_SPEED_1X, ""));
    now_playing_modes_text(&book, NULL, buffer, sizeof buffer);
    CHECK_EQUAL(0, strcmp("", buffer));
}

static void shuffle_and_repeat_are_shown_as_before(void)
{
    PlayerStatus status = {.shuffle = SHUFFLE_SONGS, .repeat = REPEAT_ONE};
    CHECK(modes_text_is(status, SPOKEN_SPEED_1X, "Shuffle Songs \xC2\xB7 Repeat One"));
}

static void the_detail_line_leaves_out_what_is_unknown(void)
{
    CHECK(detail_text_is((PlayerStatus){.place = {3, 10}}, "3 of 10"));
    CHECK(detail_text_is((PlayerStatus){.format = MP3}, "320 kbps"));
    CHECK(detail_text_is((PlayerStatus){0}, ""));
}

static void the_detail_line_shows_the_chapter_of_a_book_with_chapters(void)
{
    PlayerStatus status = {.format = MP3, .place = {1, 1}, .chapters = 13, .chapter = 2};
    CHECK(detail_text_is(status, "320 kbps \xC2\xB7 Chapter 3 of 13"));
    status.format = (HalAudioFormat){0};
    CHECK(detail_text_is(status, "Chapter 3 of 13"));
    status.chapters = 0;
    CHECK(detail_text_is(status, "1 of 1"));
}

static NowPlayingCommand controlled(Key key, KeyGesture gesture, NowPlayingControls controls)
{
    return now_playing_command_with((KeyEvent){.key = key, .gesture = gesture}, false, controls);
}

static void tap_skip_hold_volume_swaps_the_rocker_gestures(void)
{
    NowPlayingControls controls = {.rocker = ROCKER_SKIP_VOLUME};
    CHECK_EQUAL(NOW_PLAYING_NEXT, controlled(KEY_VOLUME_UP, KEY_GESTURE_PRESS, controls));
    CHECK_EQUAL(NOW_PLAYING_PREVIOUS, controlled(KEY_VOLUME_DOWN, KEY_GESTURE_PRESS, controls));
    CHECK_EQUAL(NOW_PLAYING_VOLUME_UP, controlled(KEY_VOLUME_UP, KEY_GESTURE_LONG_PRESS, controls));
    CHECK_EQUAL(NOW_PLAYING_VOLUME_DOWN, controlled(KEY_VOLUME_DOWN, KEY_GESTURE_REPEAT, controls));
}

static void hold_to_seek_moves_within_the_track(void)
{
    NowPlayingControls controls = {.rocker = ROCKER_VOLUME_SEEK};
    CHECK_EQUAL(NOW_PLAYING_VOLUME_UP, controlled(KEY_VOLUME_UP, KEY_GESTURE_PRESS, controls));
    CHECK_EQUAL(NOW_PLAYING_SEEK_FORWARD,
                controlled(KEY_VOLUME_UP, KEY_GESTURE_LONG_PRESS, controls));
    CHECK_EQUAL(NOW_PLAYING_SEEK_BACK, controlled(KEY_VOLUME_DOWN, KEY_GESTURE_REPEAT, controls));
}

static void holding_while_the_volume_shows_keeps_changing_it(void)
{
    NowPlayingControls controls = {.rocker = ROCKER_VOLUME_SKIP, .volume_shown = true};
    CHECK_EQUAL(NOW_PLAYING_VOLUME_UP, controlled(KEY_VOLUME_UP, KEY_GESTURE_LONG_PRESS, controls));
    CHECK_EQUAL(NOW_PLAYING_VOLUME_DOWN, controlled(KEY_VOLUME_DOWN, KEY_GESTURE_REPEAT, controls));
    controls.volume_shown = false;
    CHECK_EQUAL(NOW_PLAYING_NEXT, controlled(KEY_VOLUME_UP, KEY_GESTURE_LONG_PRESS, controls));
}

static void the_menu_button_can_skip_to_the_next_track(void)
{
    NowPlayingControls controls = {.menu_skips = true};
    CHECK_EQUAL(NOW_PLAYING_NEXT, controlled(KEY_MENU, KEY_GESTURE_PRESS, controls));
    CHECK_EQUAL(NOW_PLAYING_UP_NEXT, controlled(KEY_MENU, KEY_GESTURE_LONG_PRESS, controls));
    controls.menu_skips = false;
    CHECK_EQUAL(NOW_PLAYING_UP_NEXT, controlled(KEY_MENU, KEY_GESTURE_PRESS, controls));
    CHECK_EQUAL(NOW_PLAYING_EQUALIZER, controlled(KEY_MENU, KEY_GESTURE_LONG_PRESS, controls));
}

static void controls_follow_the_saved_options(void)
{
    uint8_t options[SETTINGS_OPTION_COUNT] = {0};
    options[OPTION_ROCKER] = ROCKER_VOLUME_SEEK;
    options[OPTION_MENU_KEY] = MENU_KEY_NEXT;
    NowPlayingControls controls = now_playing_controls_of(options);
    CHECK_EQUAL(ROCKER_VOLUME_SEEK, controls.rocker);
    CHECK(controls.menu_skips);
    CHECK(!now_playing_controls_of(NULL).menu_skips);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_detail_line_shows_the_chapter_of_a_book_with_chapters),
        TEST_CASE(tap_skip_hold_volume_swaps_the_rocker_gestures),
        TEST_CASE(hold_to_seek_moves_within_the_track),
        TEST_CASE(holding_while_the_volume_shows_keeps_changing_it),
        TEST_CASE(the_menu_button_can_skip_to_the_next_track),
        TEST_CASE(controls_follow_the_saved_options),
        TEST_CASE(the_codec_has_a_short_name),
        TEST_CASE(lossless_streams_show_bit_depth_and_rate),
        TEST_CASE(lossy_streams_show_only_the_rate),
        TEST_CASE(an_unknown_stream_shows_nothing),
        TEST_CASE(the_detail_line_shows_bitrate_and_place),
        TEST_CASE(the_detail_line_leaves_out_what_is_unknown),
        TEST_CASE(spoken_word_shows_its_speed_on_the_modes_line),
        TEST_CASE(the_speed_comes_before_shuffle_and_repeat),
        TEST_CASE(every_speed_has_a_short_label),
        TEST_CASE(music_and_normal_speed_show_no_speed),
        TEST_CASE(shuffle_and_repeat_are_shown_as_before),
        TEST_CASE(a_volume_tap_changes_the_volume),
        TEST_CASE(holding_volume_up_skips_forward_and_volume_down_skips_back),
        TEST_CASE(play_toggles_pause_and_a_long_play_starts_seeking),
        TEST_CASE(while_seeking_volume_keys_move_the_target),
        TEST_CASE(while_seeking_play_commits_and_back_cancels),
        TEST_CASE(the_seek_target_stays_inside_the_track),
        TEST_CASE(menu_opens_up_next_and_holding_it_opens_the_equalizer),
        TEST_CASE(back_leaves_and_a_long_back_goes_home),
        TEST_CASE(power_is_left_to_the_system),
        TEST_CASE(volume_changes_show_the_volume_bar),
        TEST_CASE(skips_and_pauses_show_a_badge_for_the_new_state),
        TEST_CASE(the_volume_bar_stays_longer_than_a_badge),
        TEST_CASE(the_volume_bar_fills_in_proportion_to_the_level),
        TEST_CASE(progress_fills_in_proportion_to_the_position),
        TEST_CASE(progress_stays_inside_the_bar),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

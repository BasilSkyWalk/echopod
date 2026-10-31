#include <string.h>

#include "check.h"
#include "screens/now_playing_screen.h"

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

static void the_detail_line_leaves_out_what_is_unknown(void)
{
    CHECK(detail_text_is((PlayerStatus){.place = {3, 10}}, "3 of 10"));
    CHECK(detail_text_is((PlayerStatus){.format = MP3}, "320 kbps"));
    CHECK(detail_text_is((PlayerStatus){0}, ""));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_codec_has_a_short_name),
        TEST_CASE(lossless_streams_show_bit_depth_and_rate),
        TEST_CASE(lossy_streams_show_only_the_rate),
        TEST_CASE(an_unknown_stream_shows_nothing),
        TEST_CASE(the_detail_line_shows_bitrate_and_place),
        TEST_CASE(the_detail_line_leaves_out_what_is_unknown),
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

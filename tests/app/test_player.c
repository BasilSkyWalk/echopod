#include <string.h>

#include "check.h"
#include "fake_hal.h"
#include "fixture_library.h"
#include "hal/storage.h"
#include "playback/resume_state.h"
#include "player.h"

enum {
    BACK_IN_BLACK = 0,
    HELLS_BELLS = 0,
    SHOOT_TO_THRILL = 1,
    HELP = 2,
    ARMY_OF_ME = 3,
    LATE_IN_TRACK_MS = 5000,
    EARLY_IN_TRACK_MS = 1000,
    SEED = 7
};

static const char HELLS_BELLS_PATH[] = "/AC DC/Back In Black/1-01 - Hells Bells.flac";
static const char SHOOT_TO_THRILL_PATH[] = "/AC DC/Back In Black/1-02 - Shoot to Thrill.flac";
static const char HELP_PATH[] = "/Various Artists/The Best of Something/01 - Help!.flac";
static const char ARMY_OF_ME_PATH[] = "/Various Artists/The Best of Something/02 - Army of Me.flac";

static FixtureLibrary fixture;
static Player player;

static BrowseQuery back_in_black(void)
{
    return (BrowseQuery){.kind = BROWSE_ALBUM_TRACKS,
                         .group = LIBRARY_ARTISTS,
                         .subject = BACK_IN_BLACK,
                         .artist = LIBRARY_NONE};
}

static Player *fresh_player(void)
{
    fake_hal_reset();
    player_init(&player, fixture_library_open(&fixture));
    return &player;
}

static Player *playing(uint32_t position)
{
    Player *started = fresh_player();
    CHECK(player_play_list(started, back_in_black(), position, SEED));
    return started;
}

static bool path_is(const char *actual, const char *expected)
{
    return actual != NULL && strcmp(expected, actual) == 0;
}

static void playing_a_list_starts_the_selected_track(void)
{
    Player *started = playing(0);
    CHECK(path_is(fake_hal_audio_playing_path(), HELLS_BELLS_PATH));
    CHECK_EQUAL(0, fake_hal_audio_play_start());
    PlayerStatus status = player_status(started);
    CHECK_EQUAL(PLAYER_PLAYING, status.state);
    CHECK(status.has_track);
    CHECK_EQUAL(HELLS_BELLS, status.track);
    CHECK_EQUAL(60000, status.duration_ms);
}

static const HalAudioFormat HI_RES_FLAC = {.codec = HAL_AUDIO_CODEC_FLAC,
                                           .sample_rate = 96000,
                                           .bits_per_sample = 24,
                                           .channels = 2,
                                           .bitrate_kbps = 2304};

static void the_status_carries_the_playing_tracks_format(void)
{
    Player *started = playing(0);
    fake_hal_audio_set_format(HI_RES_FLAC);
    HalAudioFormat format = player_status(started).format;
    CHECK_EQUAL(HAL_AUDIO_CODEC_FLAC, format.codec);
    CHECK_EQUAL(96000, format.sample_rate);
    CHECK_EQUAL(24, format.bits_per_sample);
    CHECK_EQUAL(2304, format.bitrate_kbps);
}

static void the_format_waits_for_the_audio_to_reach_the_track(void)
{
    Player *started = playing(0);
    fake_hal_audio_set_format(HI_RES_FLAC);
    fake_hal_audio_set_token(999);
    CHECK_EQUAL(HAL_AUDIO_CODEC_UNKNOWN, player_status(started).format.codec);
}

static void the_status_counts_the_place_in_the_queue(void)
{
    Player *started = playing(0);
    PlayerStatus status = player_status(started);
    CHECK_EQUAL(1, status.place.number);
    CHECK_EQUAL(2, status.place.total);
}

static void the_following_track_is_queued_for_gapless_playback(void)
{
    playing(0);
    CHECK(path_is(fake_hal_audio_queued_path(), SHOOT_TO_THRILL_PATH));
}

static void a_finished_track_moves_the_queue_on(void)
{
    Player *started = playing(0);
    fake_hal_audio_finish_track();
    player_update(started, 100);
    CHECK_EQUAL(SHOOT_TO_THRILL, player_status(started).track);
    CHECK(fake_hal_audio_queued_path() == NULL);
    CHECK_EQUAL(1, fake_hal_audio_play_count());
}

static void the_end_of_the_queue_stops_playback(void)
{
    Player *started = playing(1);
    fake_hal_audio_finish_track();
    player_update(started, 100);
    CHECK_EQUAL(PLAYER_IDLE, player_status(started).state);
}

static void repeat_one_queues_the_same_track(void)
{
    Player *started = playing(0);
    player_set_repeat(started, REPEAT_ONE);
    CHECK(path_is(fake_hal_audio_queued_path(), HELLS_BELLS_PATH));
}

static void repeat_all_wraps_back_to_the_first_track(void)
{
    Player *started = playing(1);
    player_set_repeat(started, REPEAT_ALL);
    CHECK(path_is(fake_hal_audio_queued_path(), HELLS_BELLS_PATH));
}

static void skipping_forward_plays_the_next_track(void)
{
    Player *started = playing(0);
    player_skip(started, 1);
    CHECK(path_is(fake_hal_audio_playing_path(), SHOOT_TO_THRILL_PATH));
    CHECK_EQUAL(SHOOT_TO_THRILL, player_status(started).track);
}

static void skipping_back_early_plays_the_previous_track(void)
{
    Player *started = playing(1);
    fake_hal_audio_set_position(EARLY_IN_TRACK_MS);
    player_skip(started, -1);
    CHECK(path_is(fake_hal_audio_playing_path(), HELLS_BELLS_PATH));
}

static void skipping_back_late_restarts_the_track(void)
{
    Player *started = playing(1);
    fake_hal_audio_set_position(LATE_IN_TRACK_MS);
    player_skip(started, -1);
    CHECK_EQUAL(1, fake_hal_audio_seek_count());
    CHECK_EQUAL(0, fake_hal_audio_last_seek());
    CHECK_EQUAL(SHOOT_TO_THRILL, player_status(started).track);
}

static void skipping_back_from_the_first_track_restarts_it(void)
{
    Player *started = playing(0);
    player_skip(started, -1);
    CHECK_EQUAL(1, fake_hal_audio_seek_count());
    CHECK_EQUAL(HELLS_BELLS, player_status(started).track);
}

static void seeking_moves_the_playing_track(void)
{
    Player *started = playing(0);
    player_seek(started, 42000);
    CHECK_EQUAL(1, fake_hal_audio_seek_count());
    CHECK_EQUAL(42000, fake_hal_audio_last_seek());
    CHECK_EQUAL(42000, player_status(started).position_ms);
}

static void seeking_a_resumed_track_moves_where_it_will_start(void)
{
    Player *started = playing(1);
    CHECK(player_save(started));
    Player resumed;
    player_init(&resumed, &fixture.library);
    CHECK(player_restore(&resumed));
    player_seek(&resumed, 9000);
    CHECK_EQUAL(9000, player_status(&resumed).position_ms);
    player_toggle_pause(&resumed);
    CHECK_EQUAL(9000, fake_hal_audio_play_start());
}

static void pausing_and_resuming_follow_the_toggle(void)
{
    Player *started = playing(0);
    player_toggle_pause(started);
    CHECK_EQUAL(HAL_AUDIO_PAUSED, fake_hal_audio_state());
    CHECK_EQUAL(PLAYER_PAUSED, player_status(started).state);
    player_toggle_pause(started);
    CHECK_EQUAL(HAL_AUDIO_PLAYING, fake_hal_audio_state());
    CHECK_EQUAL(PLAYER_PLAYING, player_status(started).state);
}

static void an_unplayable_track_is_skipped(void)
{
    Player *started = playing(0);
    fake_hal_audio_fail_track();
    player_update(started, 100);
    CHECK(path_is(fake_hal_audio_playing_path(), SHOOT_TO_THRILL_PATH));
    CHECK_EQUAL(PLAYER_PLAYING, player_status(started).state);
}

static void a_chosen_track_that_cannot_open_is_skipped(void)
{
    Player *started = fresh_player();
    fake_hal_audio_refuse_plays(1);
    CHECK(player_play_list(started, back_in_black(), 0, SEED));
    CHECK(path_is(fake_hal_audio_playing_path(), SHOOT_TO_THRILL_PATH));
    CHECK_EQUAL(PLAYER_PLAYING, player_status(started).state);
}

static void a_next_track_that_cannot_open_is_skipped_around_with_repeat_all(void)
{
    Player *started = fresh_player();
    player_set_repeat(started, REPEAT_ALL);
    CHECK(player_play_list(started, back_in_black(), 0, SEED));
    fake_hal_audio_fail_track();
    fake_hal_audio_refuse_plays(1);
    player_update(started, 100);
    CHECK(path_is(fake_hal_audio_playing_path(), HELLS_BELLS_PATH));
    CHECK_EQUAL(PLAYER_PLAYING, player_status(started).state);
}

static void the_player_stops_when_no_track_can_play(void)
{
    Player *started = fresh_player();
    player_set_repeat(started, REPEAT_ALL);
    fake_hal_audio_refuse_plays(100);
    CHECK(!player_play_list(started, back_in_black(), 0, SEED));
    CHECK_EQUAL(PLAYER_IDLE, player_status(started).state);
    CHECK(fake_hal_audio_play_count() <= 3);
}

static void each_refused_track_is_counted_with_its_reason(void)
{
    Player *started = fresh_player();
    fake_hal_audio_set_refusal(
        (HalAudioRefusal){.problem = HAL_AUDIO_PROBLEM_RATE, .sample_rate = 192000});
    fake_hal_audio_refuse_plays(1);
    CHECK(player_play_list(started, back_in_black(), 0, SEED));
    CHECK_EQUAL(1u, started->refusal_count);
    CHECK_EQUAL(HAL_AUDIO_PROBLEM_RATE, started->refusal.problem);
    CHECK_EQUAL(192000u, started->refusal.sample_rate);
}

static void refusals_explain_themselves(void)
{
    char text[48];
    player_refusal_text((HalAudioRefusal){.problem = HAL_AUDIO_PROBLEM_RATE, .sample_rate = 192000},
                        text, sizeof text);
    CHECK(path_is(text, "192 kHz isn't supported yet"));
    player_refusal_text((HalAudioRefusal){.problem = HAL_AUDIO_PROBLEM_RATE, .sample_rate = 176400},
                        text, sizeof text);
    CHECK(path_is(text, "176.4 kHz isn't supported yet"));
    player_refusal_text((HalAudioRefusal){.problem = HAL_AUDIO_PROBLEM_FORMAT}, text, sizeof text);
    CHECK(path_is(text, "This file type isn't supported yet"));
    player_refusal_text((HalAudioRefusal){.problem = HAL_AUDIO_PROBLEM_UNREADABLE}, text,
                        sizeof text);
    CHECK(path_is(text, "Couldn't read the track"));
}

static void pulling_the_headphones_out_pauses_playback(void)
{
    Player *started = playing(0);
    fake_hal_audio_set_jack(HAL_AUDIO_JACK_NONE);
    player_update(started, 100);
    CHECK_EQUAL(PLAYER_PAUSED, player_status(started).state);
    CHECK_EQUAL(HAL_AUDIO_PAUSED, fake_hal_audio_state());
}

static void plugging_the_headphones_back_in_does_not_resume(void)
{
    Player *started = playing(0);
    fake_hal_audio_set_jack(HAL_AUDIO_JACK_NONE);
    player_update(started, 100);
    fake_hal_audio_set_jack(HAL_AUDIO_JACK_SINGLE_ENDED);
    player_update(started, 200);
    CHECK_EQUAL(PLAYER_PAUSED, player_status(started).state);
}

static void switching_from_one_jack_to_the_other_keeps_playing(void)
{
    Player *started = playing(0);
    fake_hal_audio_set_jack(HAL_AUDIO_JACK_BALANCED);
    player_update(started, 100);
    CHECK_EQUAL(PLAYER_PLAYING, player_status(started).state);
}

static void starting_with_nothing_plugged_in_still_plays(void)
{
    fake_hal_reset();
    fake_hal_audio_set_jack(HAL_AUDIO_JACK_NONE);
    player_init(&player, fixture_library_open(&fixture));
    CHECK(player_play_list(&player, back_in_black(), 0, SEED));
    player_update(&player, 100);
    CHECK_EQUAL(PLAYER_PLAYING, player_status(&player).state);
}

static void volume_changes_are_clamped_and_sent(void)
{
    Player *started = playing(0);
    CHECK_EQUAL(PLAYER_DEFAULT_VOLUME, player_status(started).volume);
    player_change_volume(started, 100);
    CHECK_EQUAL(HAL_AUDIO_VOLUME_MAX, player_status(started).volume);
    CHECK_EQUAL(HAL_AUDIO_VOLUME_MAX, fake_hal_audio_volume());
    player_change_volume(started, -100);
    CHECK_EQUAL(0, fake_hal_audio_volume());
}

static void changing_shuffle_keeps_the_current_track_playing(void)
{
    Player *started = playing(0);
    player_set_shuffle(started, SHUFFLE_SONGS, SEED);
    CHECK_EQUAL(1, fake_hal_audio_play_count());
    CHECK_EQUAL(HELLS_BELLS, player_status(started).track);
    CHECK_EQUAL(SHUFFLE_SONGS, player_status(started).shuffle);
    CHECK(fake_hal_audio_queued_path() != NULL);
}

static void a_saved_session_resumes_paused_at_its_position(void)
{
    Player *started = playing(1);
    fake_hal_audio_set_position(42000);
    CHECK(player_save(started));
    int plays_before = fake_hal_audio_play_count();
    Player resumed;
    player_init(&resumed, &fixture.library);
    CHECK(player_restore(&resumed));
    PlayerStatus status = player_status(&resumed);
    CHECK_EQUAL(PLAYER_PAUSED, status.state);
    CHECK_EQUAL(SHOOT_TO_THRILL, status.track);
    CHECK_EQUAL(42000, status.position_ms);
    CHECK_EQUAL(plays_before, fake_hal_audio_play_count());
    player_toggle_pause(&resumed);
    CHECK(path_is(fake_hal_audio_playing_path(), SHOOT_TO_THRILL_PATH));
    CHECK_EQUAL(42000, fake_hal_audio_play_start());
}

static void a_session_from_another_library_is_ignored(void)
{
    fresh_player();
    ResumeState foreign = {.library_fingerprint = 1, .plan = {.source = back_in_black()}};
    uint8_t bytes[RESUME_STATE_SIZE];
    resume_state_encode(&foreign, bytes);
    hal_file_write("/.echo/resume.bin", bytes, sizeof bytes);
    CHECK(!player_restore(&player));
    CHECK_EQUAL(PLAYER_IDLE, player_status(&player).state);
}

static void the_session_is_saved_regularly_while_playing(void)
{
    Player *started = playing(0);
    player_update(started, 1000);
    int writes = fake_hal_write_count();
    player_update(started, 1000 + PLAYER_SAVE_INTERVAL_MS);
    CHECK_EQUAL(writes + 1, fake_hal_write_count());
}

static void finish_playing(Player *started)
{
    fake_hal_audio_finish_track();
    player_update(started, 100);
}

static void an_added_song_plays_after_the_current_track(void)
{
    Player *started = playing(0);
    CHECK_EQUAL(PLAYER_UP_NEXT_ADDED, player_add_to_up_next(started, HELP));
    CHECK(path_is(fake_hal_audio_queued_path(), HELP_PATH));
    finish_playing(started);
    CHECK_EQUAL(HELP, player_status(started).track);
    CHECK_EQUAL(0, started->up_next.count);
    CHECK(path_is(fake_hal_audio_queued_path(), SHOOT_TO_THRILL_PATH));
    finish_playing(started);
    CHECK_EQUAL(SHOOT_TO_THRILL, player_status(started).track);
}

static void play_next_goes_before_earlier_added_songs(void)
{
    Player *started = playing(0);
    player_add_to_up_next(started, HELP);
    CHECK_EQUAL(PLAYER_UP_NEXT_ADDED, player_play_next(started, ARMY_OF_ME));
    CHECK(path_is(fake_hal_audio_queued_path(), ARMY_OF_ME_PATH));
    finish_playing(started);
    CHECK_EQUAL(ARMY_OF_ME, player_status(started).track);
    CHECK(path_is(fake_hal_audio_queued_path(), HELP_PATH));
}

static void skipping_forward_takes_the_next_added_song(void)
{
    Player *started = playing(0);
    player_add_to_up_next(started, HELP);
    player_skip(started, 1);
    CHECK(path_is(fake_hal_audio_playing_path(), HELP_PATH));
    player_skip(started, 1);
    CHECK(path_is(fake_hal_audio_playing_path(), SHOOT_TO_THRILL_PATH));
}

static void skipping_back_from_an_added_song_returns_to_the_list(void)
{
    Player *started = playing(0);
    player_add_to_up_next(started, HELP);
    player_skip(started, 1);
    fake_hal_audio_set_position(EARLY_IN_TRACK_MS);
    player_skip(started, -1);
    CHECK(path_is(fake_hal_audio_playing_path(), HELLS_BELLS_PATH));
    CHECK(path_is(fake_hal_audio_queued_path(), SHOOT_TO_THRILL_PATH));
}

static void adding_with_nothing_playing_starts_the_song(void)
{
    Player *idle = fresh_player();
    CHECK_EQUAL(PLAYER_UP_NEXT_STARTED, player_add_to_up_next(idle, HELP));
    CHECK(path_is(fake_hal_audio_playing_path(), HELP_PATH));
    CHECK_EQUAL(PLAYER_PLAYING, player_status(idle).state);
    finish_playing(idle);
    CHECK_EQUAL(PLAYER_IDLE, player_status(idle).state);
}

static void a_full_up_next_refuses_more(void)
{
    Player *started = playing(0);
    for (int index = 0; index < UP_NEXT_CAPACITY; index++) {
        CHECK_EQUAL(PLAYER_UP_NEXT_ADDED, player_add_to_up_next(started, HELP));
    }
    CHECK_EQUAL(PLAYER_UP_NEXT_FULL, player_add_to_up_next(started, ARMY_OF_ME));
    CHECK_EQUAL(PLAYER_UP_NEXT_FULL, player_play_next(started, ARMY_OF_ME));
}

static void a_new_list_keeps_up_next(void)
{
    Player *started = playing(0);
    player_add_to_up_next(started, HELP);
    CHECK(player_play_list(started, back_in_black(), 1, SEED));
    CHECK(path_is(fake_hal_audio_playing_path(), SHOOT_TO_THRILL_PATH));
    CHECK(path_is(fake_hal_audio_queued_path(), HELP_PATH));
}

static void repeat_one_repeats_an_added_song(void)
{
    Player *started = playing(0);
    player_add_to_up_next(started, HELP);
    player_skip(started, 1);
    player_set_repeat(started, REPEAT_ONE);
    CHECK(path_is(fake_hal_audio_queued_path(), HELP_PATH));
    finish_playing(started);
    CHECK_EQUAL(HELP, player_status(started).track);
    CHECK(path_is(fake_hal_audio_queued_path(), HELP_PATH));
}

static void jumping_to_an_added_song_skips_the_ones_before_it(void)
{
    Player *started = playing(0);
    player_add_to_up_next(started, HELP);
    player_add_to_up_next(started, ARMY_OF_ME);
    CHECK(player_jump_to_up_next(started, 1));
    CHECK(path_is(fake_hal_audio_playing_path(), ARMY_OF_ME_PATH));
    CHECK_EQUAL(0, started->up_next.count);
    CHECK(path_is(fake_hal_audio_queued_path(), SHOOT_TO_THRILL_PATH));
}

static void jumping_into_the_list_clears_up_next(void)
{
    Player *started = playing(0);
    player_add_to_up_next(started, HELP);
    CHECK(player_jump_to_queue(started, (PlayQueueCursor){1, 0}));
    CHECK(path_is(fake_hal_audio_playing_path(), SHOOT_TO_THRILL_PATH));
    CHECK_EQUAL(0, started->up_next.count);
}

static void up_next_survives_a_restart(void)
{
    Player *started = playing(0);
    player_add_to_up_next(started, HELP);
    player_add_to_up_next(started, ARMY_OF_ME);
    Player resumed;
    player_init(&resumed, &fixture.library);
    CHECK(player_restore(&resumed));
    CHECK_EQUAL(2, resumed.up_next.count);
    CHECK_EQUAL(HELP, resumed.up_next.tracks[0]);
    CHECK_EQUAL(ARMY_OF_ME, resumed.up_next.tracks[1]);
}

static void a_session_resumes_inside_an_added_song(void)
{
    Player *started = playing(0);
    player_add_to_up_next(started, HELP);
    player_skip(started, 1);
    fake_hal_audio_set_position(42000);
    CHECK(player_save(started));
    Player resumed;
    player_init(&resumed, &fixture.library);
    CHECK(player_restore(&resumed));
    CHECK_EQUAL(HELP, player_status(&resumed).track);
    player_toggle_pause(&resumed);
    CHECK(path_is(fake_hal_audio_playing_path(), HELP_PATH));
    CHECK_EQUAL(42000, fake_hal_audio_play_start());
    CHECK(path_is(fake_hal_audio_queued_path(), SHOOT_TO_THRILL_PATH));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(playing_a_list_starts_the_selected_track),
        TEST_CASE(the_status_carries_the_playing_tracks_format),
        TEST_CASE(the_format_waits_for_the_audio_to_reach_the_track),
        TEST_CASE(the_status_counts_the_place_in_the_queue),
        TEST_CASE(the_following_track_is_queued_for_gapless_playback),
        TEST_CASE(a_finished_track_moves_the_queue_on),
        TEST_CASE(the_end_of_the_queue_stops_playback),
        TEST_CASE(repeat_one_queues_the_same_track),
        TEST_CASE(repeat_all_wraps_back_to_the_first_track),
        TEST_CASE(skipping_forward_plays_the_next_track),
        TEST_CASE(skipping_back_early_plays_the_previous_track),
        TEST_CASE(skipping_back_late_restarts_the_track),
        TEST_CASE(skipping_back_from_the_first_track_restarts_it),
        TEST_CASE(seeking_moves_the_playing_track),
        TEST_CASE(seeking_a_resumed_track_moves_where_it_will_start),
        TEST_CASE(pausing_and_resuming_follow_the_toggle),
        TEST_CASE(an_unplayable_track_is_skipped),
        TEST_CASE(a_chosen_track_that_cannot_open_is_skipped),
        TEST_CASE(a_next_track_that_cannot_open_is_skipped_around_with_repeat_all),
        TEST_CASE(the_player_stops_when_no_track_can_play),
        TEST_CASE(each_refused_track_is_counted_with_its_reason),
        TEST_CASE(refusals_explain_themselves),
        TEST_CASE(pulling_the_headphones_out_pauses_playback),
        TEST_CASE(plugging_the_headphones_back_in_does_not_resume),
        TEST_CASE(switching_from_one_jack_to_the_other_keeps_playing),
        TEST_CASE(starting_with_nothing_plugged_in_still_plays),
        TEST_CASE(volume_changes_are_clamped_and_sent),
        TEST_CASE(changing_shuffle_keeps_the_current_track_playing),
        TEST_CASE(a_saved_session_resumes_paused_at_its_position),
        TEST_CASE(a_session_from_another_library_is_ignored),
        TEST_CASE(the_session_is_saved_regularly_while_playing),
        TEST_CASE(an_added_song_plays_after_the_current_track),
        TEST_CASE(play_next_goes_before_earlier_added_songs),
        TEST_CASE(skipping_forward_takes_the_next_added_song),
        TEST_CASE(skipping_back_from_an_added_song_returns_to_the_list),
        TEST_CASE(adding_with_nothing_playing_starts_the_song),
        TEST_CASE(a_full_up_next_refuses_more),
        TEST_CASE(a_new_list_keeps_up_next),
        TEST_CASE(repeat_one_repeats_an_added_song),
        TEST_CASE(jumping_to_an_added_song_skips_the_ones_before_it),
        TEST_CASE(jumping_into_the_list_clears_up_next),
        TEST_CASE(up_next_survives_a_restart),
        TEST_CASE(a_session_resumes_inside_an_added_song),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

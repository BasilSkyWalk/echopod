#include <string.h>

#include "bluetooth_link.h"
#include "bluetooth_store.h"
#include "check.h"
#include "fake_hal.h"
#include "fixture_library.h"
#include "player.h"

enum {
    START_MS = 1000,
    BACK_IN_BLACK = 0,
    SEED = 7
};

static const HalBluetoothDevice BUDS = {.address = {1, 2, 3, 4, 5, 6}, .name = "Buds"};
static const HalBluetoothDevice SPEAKER = {.address = {9, 8, 7, 6, 5, 4}, .name = "Speaker"};

static FixtureLibrary fixture;
static Player player;
static Toast toast;
static BluetoothLink link;

static void fresh(void)
{
    fake_hal_reset();
    fake_hal_audio_set_jack(HAL_AUDIO_JACK_NONE);
    player_init(&player, fixture_library_open(&fixture));
    toast = (Toast){0};
}

static void save(bool enabled, const HalBluetoothDevice *device)
{
    BluetoothSaved saved = {.enabled = enabled, .count = device != NULL ? 1 : 0};
    if (device != NULL) {
        saved.devices[0] = *device;
    }
    bluetooth_store_save(&saved);
}

static void start(void)
{
    bluetooth_link_start(&link, &toast, START_MS);
}

static void step(uint32_t now_ms)
{
    bluetooth_link_step(&link, &player, now_ms);
}

static void play(void)
{
    BrowseQuery album = {.kind = BROWSE_ALBUM_TRACKS,
                         .group = LIBRARY_ARTISTS,
                         .subject = BACK_IN_BLACK,
                         .artist = LIBRARY_NONE};
    CHECK(player_play_list(&player, album, 0, SEED));
}

static void ready(void)
{
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_READY;
}

static void connect_to(const HalBluetoothDevice *device)
{
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_CONNECTED;
    fake_hal_bluetooth()->connected = *device;
}

static BluetoothSaved stored(void)
{
    BluetoothSaved saved;
    bluetooth_store_load(&saved);
    return saved;
}

static bool summary_is(const char *text)
{
    return strcmp(text, bluetooth_link_summary(&link)) == 0;
}

static bool toast_is(const char *text)
{
    return toast.visible && strcmp(text, toast.text) == 0;
}

static void starts_off_when_never_turned_on(void)
{
    fresh();
    start();
    step(START_MS);
    CHECK(!fake_hal_bluetooth_powered());
    CHECK(summary_is("Off"));
}

static void turning_on_powers_the_module_and_is_remembered(void)
{
    fresh();
    start();
    bluetooth_link_set_enabled(&link, true, START_MS);
    CHECK(fake_hal_bluetooth_powered());
    CHECK(stored().enabled);
    CHECK(summary_is("Starting…"));
}

static void turning_off_powers_the_module_down(void)
{
    fresh();
    save(true, NULL);
    start();
    bluetooth_link_set_enabled(&link, false, START_MS + 100);
    CHECK(!fake_hal_bluetooth_powered());
    CHECK(!stored().enabled);
    CHECK(summary_is("Off"));
}

static void power_on_reconnects_the_saved_device(void)
{
    fresh();
    save(true, &BUDS);
    start();
    CHECK(fake_hal_bluetooth_powered());
    ready();
    step(START_MS + 100);
    CHECK_EQUAL(1, fake_hal_bluetooth_connect_count());
    CHECK(strcmp("Buds", fake_hal_bluetooth_connect_target()->name) == 0);
}

static void a_failed_reconnect_is_retried_later(void)
{
    fresh();
    save(true, &BUDS);
    start();
    ready();
    step(START_MS + 100);
    step(START_MS + 200);
    CHECK_EQUAL(1, fake_hal_bluetooth_connect_count());
    step(START_MS + 100 + BLUETOOTH_LINK_RETRY_MS);
    CHECK_EQUAL(2, fake_hal_bluetooth_connect_count());
}

static void a_paired_reply_is_waited_for_without_searching(void)
{
    fresh();
    save(true, &BUDS);
    start();
    bluetooth_link_browse(&link, true, START_MS);
    ready();
    step(START_MS + 100);
    CHECK_EQUAL(1, fake_hal_bluetooth_connect_count());
    fake_hal_bluetooth()->pairing = HAL_BLUETOOTH_PAIRED;
    step(START_MS + 1000);
    step(START_MS + 100 + BLUETOOTH_LINK_CONFIRM_MS - 1);
    CHECK_EQUAL(0, fake_hal_bluetooth_scan_count());
    CHECK_EQUAL(1, fake_hal_bluetooth_connect_count());
    CHECK_EQUAL(0, fake_hal_bluetooth_disconnect_count());
}

static void a_stalled_pairing_is_reset_and_tried_again(void)
{
    fresh();
    save(true, &BUDS);
    start();
    ready();
    step(START_MS + 100);
    CHECK_EQUAL(1, fake_hal_bluetooth_connect_count());
    fake_hal_bluetooth()->pairing = HAL_BLUETOOTH_PAIRED;
    step(START_MS + 1000);
    step(START_MS + 1000 + BLUETOOTH_LINK_CONFIRM_MS - 1);
    CHECK_EQUAL(0, fake_hal_bluetooth_disconnect_count());
    uint32_t stalled_ms = START_MS + 1000 + BLUETOOTH_LINK_CONFIRM_MS;
    step(stalled_ms);
    CHECK_EQUAL(1, fake_hal_bluetooth_disconnect_count());
    fake_hal_bluetooth()->pairing = HAL_BLUETOOTH_PAIRING_NONE;
    step(stalled_ms + BLUETOOTH_LINK_RESET_MS - 1);
    CHECK_EQUAL(1, fake_hal_bluetooth_connect_count());
    step(stalled_ms + BLUETOOTH_LINK_RESET_MS);
    CHECK_EQUAL(2, fake_hal_bluetooth_connect_count());
    CHECK(strcmp("Buds", fake_hal_bluetooth_connect_target()->name) == 0);
}

static void a_search_and_a_reconnect_take_turns(void)
{
    fresh();
    save(true, &BUDS);
    start();
    bluetooth_link_browse(&link, true, START_MS);
    ready();
    step(START_MS + 100);
    CHECK_EQUAL(1, fake_hal_bluetooth_connect_count());
    fake_hal_bluetooth()->pairing = HAL_BLUETOOTH_PAIRING_FAILED;
    step(START_MS + 200);
    CHECK_EQUAL(1, fake_hal_bluetooth_scan_count());
    CHECK_EQUAL(HAL_BLUETOOTH_SCANNING, fake_hal_bluetooth()->state);
    step(START_MS + 100 + 2 * BLUETOOTH_LINK_RETRY_MS);
    CHECK_EQUAL(1, fake_hal_bluetooth_connect_count());
    CHECK(summary_is("Searching…"));
    ready();
    step(START_MS + 100 + 2 * BLUETOOTH_LINK_RETRY_MS + 100);
    CHECK_EQUAL(2, fake_hal_bluetooth_connect_count());
    CHECK_EQUAL(1, fake_hal_bluetooth_scan_count());
}

static void nothing_is_retried_while_connecting(void)
{
    fresh();
    save(true, &BUDS);
    start();
    ready();
    step(START_MS + 100);
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_CONNECTING;
    step(START_MS + 100 + BLUETOOTH_LINK_RETRY_MS);
    CHECK_EQUAL(1, fake_hal_bluetooth_connect_count());
}

static void connecting_saves_the_device_and_says_so(void)
{
    fresh();
    save(true, NULL);
    start();
    connect_to(&SPEAKER);
    step(START_MS + 100);
    BluetoothSaved saved = stored();
    CHECK_EQUAL(1, saved.count);
    CHECK(strcmp("Speaker", saved.devices[0].name) == 0);
    CHECK(toast_is("Connected to Speaker"));
    CHECK(summary_is("Speaker"));
}

static void playing_with_nothing_plugged_in_streams(void)
{
    fresh();
    save(true, &BUDS);
    start();
    connect_to(&BUDS);
    step(START_MS + 100);
    CHECK(!fake_hal_bluetooth()->streaming);
    play();
    step(START_MS + 200);
    CHECK(fake_hal_bluetooth()->streaming);
    player_toggle_pause(&player);
    step(START_MS + 300);
    CHECK(!fake_hal_bluetooth()->streaming);
}

static void headphones_take_the_sound_back(void)
{
    fresh();
    save(true, &BUDS);
    start();
    connect_to(&BUDS);
    play();
    step(START_MS + 100);
    CHECK(fake_hal_bluetooth()->streaming);
    fake_hal_audio_set_jack(HAL_AUDIO_JACK_SINGLE_ENDED);
    step(START_MS + 200);
    CHECK(!fake_hal_bluetooth()->streaming);
    CHECK_EQUAL(PLAYER_PLAYING, player.state);
}

static void the_buds_wait_for_the_next_play_after_headphones(void)
{
    fresh();
    save(true, &BUDS);
    start();
    connect_to(&BUDS);
    play();
    fake_hal_audio_set_jack(HAL_AUDIO_JACK_SINGLE_ENDED);
    step(START_MS + 100);
    fake_hal_audio_set_jack(HAL_AUDIO_JACK_NONE);
    step(START_MS + 200);
    CHECK(!fake_hal_bluetooth()->streaming);
    player_toggle_pause(&player);
    step(START_MS + 300);
    player_toggle_pause(&player);
    step(START_MS + 400);
    CHECK(fake_hal_bluetooth()->streaming);
}

static void the_buds_pause_and_resume_playback(void)
{
    fresh();
    save(true, &BUDS);
    start();
    connect_to(&BUDS);
    play();
    step(START_MS + 100);
    fake_hal_bluetooth()->remote.pauses++;
    step(START_MS + 100 + BLUETOOTH_LINK_ECHO_MS);
    CHECK_EQUAL(PLAYER_PAUSED, player.state);
    CHECK(!fake_hal_bluetooth()->streaming);
    fake_hal_bluetooth()->remote.plays++;
    step(START_MS + 100 + 2 * BLUETOOTH_LINK_ECHO_MS);
    CHECK_EQUAL(PLAYER_PLAYING, player.state);
    CHECK(fake_hal_bluetooth()->streaming);
}

static void reports_right_after_the_stream_starts_are_ignored(void)
{
    fresh();
    save(true, &BUDS);
    start();
    connect_to(&BUDS);
    play();
    step(START_MS + 100);
    fake_hal_bluetooth()->remote.pauses++;
    step(START_MS + 200);
    CHECK_EQUAL(PLAYER_PLAYING, player.state);
    CHECK(fake_hal_bluetooth()->streaming);
}

static void a_play_report_does_not_resume_what_the_player_paused(void)
{
    fresh();
    save(true, &BUDS);
    start();
    connect_to(&BUDS);
    play();
    step(START_MS + 100);
    player_toggle_pause(&player);
    step(START_MS + 200);
    fake_hal_bluetooth()->remote.plays++;
    step(START_MS + 200 + BLUETOOTH_LINK_ECHO_MS);
    CHECK_EQUAL(PLAYER_PAUSED, player.state);
}

static void the_buds_skip_tracks(void)
{
    fresh();
    save(true, &BUDS);
    start();
    connect_to(&BUDS);
    play();
    step(START_MS + 100);
    uint16_t first = player.track;
    fake_hal_bluetooth()->remote.nexts++;
    step(START_MS + 200);
    CHECK(player.track != first);
    fake_hal_bluetooth()->remote.previouses++;
    step(START_MS + 300);
    CHECK_EQUAL(first, player.track);
}

static void presses_from_before_the_connection_are_ignored(void)
{
    fresh();
    save(true, &BUDS);
    start();
    play();
    fake_hal_bluetooth()->remote.nexts = 3;
    uint16_t first = player.track;
    connect_to(&BUDS);
    step(START_MS + 100);
    CHECK_EQUAL(first, player.track);
}

static void losing_the_buds_pauses_playback(void)
{
    fresh();
    save(true, &BUDS);
    start();
    connect_to(&BUDS);
    play();
    step(START_MS + 100);
    ready();
    step(START_MS + 200);
    CHECK_EQUAL(PLAYER_PAUSED, player.state);
    CHECK(toast_is("Disconnected from Buds"));
}

static void an_unused_module_switches_itself_off(void)
{
    fresh();
    save(true, &BUDS);
    start();
    ready();
    step(START_MS + BLUETOOTH_LINK_IDLE_OFF_MS - 1);
    CHECK(fake_hal_bluetooth_powered());
    step(START_MS + BLUETOOTH_LINK_IDLE_OFF_MS);
    CHECK(!fake_hal_bluetooth_powered());
    CHECK(stored().enabled);
    CHECK(summary_is("Not connected"));
}

static void browsing_wakes_a_dozing_module(void)
{
    fresh();
    save(true, &BUDS);
    start();
    step(START_MS + BLUETOOTH_LINK_IDLE_OFF_MS);
    CHECK(!fake_hal_bluetooth_powered());
    bluetooth_link_browse(&link, true, START_MS + BLUETOOTH_LINK_IDLE_OFF_MS + 100);
    CHECK(fake_hal_bluetooth_powered());
    step(START_MS + 3 * BLUETOOTH_LINK_IDLE_OFF_MS);
    CHECK(fake_hal_bluetooth_powered());
}

static void browsing_searches_for_devices(void)
{
    fresh();
    save(true, NULL);
    start();
    bluetooth_link_browse(&link, true, START_MS);
    ready();
    step(START_MS + 100);
    CHECK_EQUAL(1, fake_hal_bluetooth_scan_count());
    CHECK(summary_is("Searching…"));
}

static void pairing_connects_the_chosen_device(void)
{
    fresh();
    save(true, NULL);
    start();
    ready();
    bluetooth_link_pair(&link, &SPEAKER, START_MS + 100);
    step(START_MS + 100);
    CHECK_EQUAL(1, fake_hal_bluetooth_connect_count());
    CHECK(strcmp("Speaker", fake_hal_bluetooth_connect_target()->name) == 0);
}

static void devices_are_remembered_most_recent_first(void)
{
    fresh();
    save(true, &BUDS);
    start();
    connect_to(&SPEAKER);
    step(START_MS + 100);
    BluetoothSaved saved = stored();
    CHECK_EQUAL(2, saved.count);
    CHECK(strcmp("Speaker", saved.devices[0].name) == 0);
    CHECK(strcmp("Buds", saved.devices[1].name) == 0);
}

static void choosing_the_connected_device_disconnects_and_stays_off(void)
{
    fresh();
    save(true, &BUDS);
    start();
    connect_to(&BUDS);
    step(START_MS + 100);
    bluetooth_link_choose(&link, 0, START_MS + 200);
    CHECK_EQUAL(1, fake_hal_bluetooth_disconnect_count());
    ready();
    step(START_MS + 300);
    step(START_MS + 300 + 3 * BLUETOOTH_LINK_RETRY_MS);
    CHECK_EQUAL(0, fake_hal_bluetooth_connect_count());
    CHECK(summary_is("Not connected"));
    bluetooth_link_choose(&link, 0, START_MS + 400 + 3 * BLUETOOTH_LINK_RETRY_MS);
    step(START_MS + 400 + 3 * BLUETOOTH_LINK_RETRY_MS);
    CHECK_EQUAL(1, fake_hal_bluetooth_connect_count());
}

static void choosing_another_device_switches_to_it(void)
{
    fresh();
    BluetoothSaved two = {.enabled = true, .count = 2, .devices = {BUDS, SPEAKER}};
    bluetooth_store_save(&two);
    start();
    connect_to(&BUDS);
    step(START_MS + 100);
    bluetooth_link_choose(&link, 1, START_MS + 200);
    CHECK_EQUAL(1, fake_hal_bluetooth_disconnect_count());
    CHECK_EQUAL(BLUETOOTH_DEVICE_CONNECTING, bluetooth_link_device_state(&link, 1));
    ready();
    step(START_MS + 300);
    step(START_MS + 200 + BLUETOOTH_LINK_RESET_MS);
    CHECK_EQUAL(1, fake_hal_bluetooth_connect_count());
    CHECK(strcmp("Speaker", fake_hal_bluetooth_connect_target()->name) == 0);
}

static void searching_disconnects_and_scans_without_reconnecting(void)
{
    fresh();
    save(true, &BUDS);
    start();
    connect_to(&BUDS);
    step(START_MS + 100);
    bluetooth_link_search(&link, START_MS + 200);
    CHECK_EQUAL(1, fake_hal_bluetooth_disconnect_count());
    ready();
    step(START_MS + 300);
    CHECK_EQUAL(1, fake_hal_bluetooth_scan_count());
    ready();
    step(START_MS + 300 + 2 * BLUETOOTH_LINK_RETRY_MS);
    CHECK_EQUAL(0, fake_hal_bluetooth_connect_count());
}

static void an_old_single_device_store_still_loads(void)
{
    fresh();
    uint8_t old[42] = {'B', 'T', 1, 1, 2, 3, 4, 5, 6, 'B', 'u', 'd', 's'};
    old[41] = 1;
    fake_hal_add_file(BLUETOOTH_STORE_PATH, old, sizeof old);
    BluetoothSaved saved = stored();
    CHECK(saved.enabled);
    CHECK_EQUAL(1, saved.count);
    CHECK(strcmp("Buds", saved.devices[0].name) == 0);
    CHECK_EQUAL(6, saved.devices[0].address[5]);
}

static void forgetting_disconnects_and_clears_the_device(void)
{
    fresh();
    save(true, &BUDS);
    start();
    connect_to(&BUDS);
    step(START_MS + 100);
    bluetooth_link_forget(&link, 0);
    CHECK_EQUAL(1, fake_hal_bluetooth_disconnect_count());
    CHECK_EQUAL(0, stored().count);
    CHECK(stored().enabled);
    step(START_MS + 200);
    CHECK_EQUAL(0, stored().count);
    CHECK(summary_is("On"));
}

int main(void)
{
    const TestCase cases[] = {
        TEST_CASE(starts_off_when_never_turned_on),
        TEST_CASE(turning_on_powers_the_module_and_is_remembered),
        TEST_CASE(turning_off_powers_the_module_down),
        TEST_CASE(power_on_reconnects_the_saved_device),
        TEST_CASE(a_failed_reconnect_is_retried_later),
        TEST_CASE(a_paired_reply_is_waited_for_without_searching),
        TEST_CASE(a_stalled_pairing_is_reset_and_tried_again),
        TEST_CASE(nothing_is_retried_while_connecting),
        TEST_CASE(a_search_and_a_reconnect_take_turns),
        TEST_CASE(connecting_saves_the_device_and_says_so),
        TEST_CASE(playing_with_nothing_plugged_in_streams),
        TEST_CASE(headphones_take_the_sound_back),
        TEST_CASE(the_buds_wait_for_the_next_play_after_headphones),
        TEST_CASE(the_buds_pause_and_resume_playback),
        TEST_CASE(reports_right_after_the_stream_starts_are_ignored),
        TEST_CASE(a_play_report_does_not_resume_what_the_player_paused),
        TEST_CASE(the_buds_skip_tracks),
        TEST_CASE(presses_from_before_the_connection_are_ignored),
        TEST_CASE(losing_the_buds_pauses_playback),
        TEST_CASE(an_unused_module_switches_itself_off),
        TEST_CASE(browsing_wakes_a_dozing_module),
        TEST_CASE(browsing_searches_for_devices),
        TEST_CASE(pairing_connects_the_chosen_device),
        TEST_CASE(devices_are_remembered_most_recent_first),
        TEST_CASE(choosing_the_connected_device_disconnects_and_stays_off),
        TEST_CASE(choosing_another_device_switches_to_it),
        TEST_CASE(searching_disconnects_and_scans_without_reconnecting),
        TEST_CASE(an_old_single_device_store_still_loads),
        TEST_CASE(forgetting_disconnects_and_clears_the_device),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

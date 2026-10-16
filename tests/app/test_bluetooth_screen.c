#include <string.h>

#include "check.h"
#include "default_theme.h"
#include "fake_hal.h"
#include "screens/bluetooth_screen.h"

enum {
    START_MS = 1000
};

static BluetoothScreen bluetooth;

static Screen *open_bluetooth(void)
{
    fake_hal_reset();
    Screen *screen = bluetooth_screen_init(&bluetooth, &DEFAULT_THEME);
    screen->enter(screen->context, START_MS);
    return screen;
}

static KeyEvent tap(Key key)
{
    return (KeyEvent){.key = key, .gesture = KEY_GESTURE_PRESS};
}

static KeyEvent hold(Key key)
{
    return (KeyEvent){.key = key, .gesture = KEY_GESTURE_LONG_PRESS};
}

static bool line_is(int index, const char *text)
{
    return strcmp(text, bluetooth_screen_line(&bluetooth, index)) == 0;
}

static void entering_powers_the_module_on(void)
{
    Screen *screen = open_bluetooth();
    screen->update(screen->context, START_MS);
    CHECK(fake_hal_bluetooth_powered());
    CHECK(line_is(0, "Starting"));
}

static void a_ready_module_starts_one_scan(void)
{
    Screen *screen = open_bluetooth();
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_READY;
    screen->update(screen->context, START_MS + 100);
    CHECK_EQUAL(1, fake_hal_bluetooth_scan_count());
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_READY;
    screen->update(screen->context, START_MS + 200);
    CHECK_EQUAL(1, fake_hal_bluetooth_scan_count());
}

static void play_scans_again(void)
{
    Screen *screen = open_bluetooth();
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_READY;
    screen->update(screen->context, START_MS + 100);
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_READY;
    screen->handle_key(screen->context, tap(KEY_PLAY), START_MS + 200);
    CHECK_EQUAL(2, fake_hal_bluetooth_scan_count());
}

static void back_powers_the_module_off_and_leaves(void)
{
    Screen *screen = open_bluetooth();
    ScreenAction action = screen->handle_key(screen->context, tap(KEY_BACK), START_MS + 100);
    CHECK_EQUAL(SCREEN_ACTION_POP, action.type);
    CHECK(!fake_hal_bluetooth_powered());
}

static void the_state_line_shows_the_version_and_link_status(void)
{
    Screen *screen = open_bluetooth();
    HalBluetoothStatus *status = fake_hal_bluetooth();
    status->state = HAL_BLUETOOTH_SCANNING;
    status->version = '5';
    status->link_status = 'I';
    screen->update(screen->context, START_MS + 100);
    CHECK(line_is(0, "Scanning · v5 · status I"));
}

static void no_reply_is_reported(void)
{
    Screen *screen = open_bluetooth();
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_NO_REPLY;
    screen->update(screen->context, START_MS + 100);
    CHECK(line_is(0, "No reply"));
}

static void the_link_line_counts_bytes_frames_and_overruns(void)
{
    Screen *screen = open_bluetooth();
    HalBluetoothLink *link = &fake_hal_bluetooth()->link;
    link->received = 123;
    link->frames = 9;
    link->overruns = 2;
    link->fifo_peak = 7;
    link->fifo_depth = 16;
    screen->update(screen->context, START_MS + 100);
    CHECK(line_is(1, "RX 123 · 9 frames · OE 2 · FIFO 7/16"));
}

static void the_last_bytes_show_in_hex(void)
{
    Screen *screen = open_bluetooth();
    HalBluetoothLink *link = &fake_hal_bluetooth()->link;
    link->recent[0] = 0x43;
    link->recent[1] = 0x56;
    link->recent[2] = 0x0A;
    link->recent_length = 3;
    screen->update(screen->context, START_MS + 100);
    CHECK(line_is(2, "43 56 0A"));
}

static void nothing_received_says_so(void)
{
    Screen *screen = open_bluetooth();
    screen->update(screen->context, START_MS + 100);
    CHECK(line_is(2, "Nothing received"));
}

static void found_devices_are_listed(void)
{
    Screen *screen = open_bluetooth();
    HalBluetoothStatus *status = fake_hal_bluetooth();
    status->device_count = 2;
    strcpy(status->devices[0].name, "WH-1000XM4");
    strcpy(status->devices[1].name, "Kitchen");
    screen->update(screen->context, START_MS + 100);
    CHECK(line_is(3, "› 1. WH-1000XM4"));
    CHECK(line_is(4, "2. Kitchen"));
    CHECK(line_is(5, ""));
}

static Screen *open_with_two_devices(void)
{
    Screen *screen = open_bluetooth();
    HalBluetoothStatus *status = fake_hal_bluetooth();
    status->state = HAL_BLUETOOTH_READY;
    status->device_count = 2;
    strcpy(status->devices[0].name, "WH-1000XM4");
    status->devices[0].address[0] = 0x11;
    strcpy(status->devices[1].name, "Kitchen");
    status->devices[1].address[0] = 0x22;
    screen->update(screen->context, START_MS + 100);
    return screen;
}

static void volume_keys_move_the_selection_and_wrap(void)
{
    Screen *screen = open_with_two_devices();
    screen->handle_key(screen->context, tap(KEY_VOLUME_UP), START_MS + 200);
    screen->update(screen->context, START_MS + 200);
    CHECK(line_is(4, "› 2. Kitchen"));
    screen->handle_key(screen->context, tap(KEY_VOLUME_UP), START_MS + 300);
    screen->update(screen->context, START_MS + 300);
    CHECK(line_is(3, "› 1. WH-1000XM4"));
    screen->handle_key(screen->context, tap(KEY_VOLUME_DOWN), START_MS + 400);
    screen->update(screen->context, START_MS + 400);
    CHECK(line_is(4, "› 2. Kitchen"));
}

static void play_pairs_the_selected_device(void)
{
    Screen *screen = open_with_two_devices();
    screen->handle_key(screen->context, tap(KEY_VOLUME_UP), START_MS + 200);
    screen->handle_key(screen->context, tap(KEY_PLAY), START_MS + 300);
    CHECK_EQUAL(1, fake_hal_bluetooth_connect_count());
    CHECK_EQUAL(1, fake_hal_bluetooth_connect_index());
    CHECK_EQUAL(1, fake_hal_bluetooth_scan_count());
}

static void holding_play_scans_again(void)
{
    Screen *screen = open_with_two_devices();
    screen->handle_key(screen->context, hold(KEY_PLAY), START_MS + 300);
    CHECK_EQUAL(2, fake_hal_bluetooth_scan_count());
    CHECK_EQUAL(0, fake_hal_bluetooth_connect_count());
}

static void holding_play_while_connected_disconnects(void)
{
    Screen *screen = open_with_two_devices();
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_CONNECTED;
    screen->handle_key(screen->context, hold(KEY_PLAY), START_MS + 300);
    CHECK_EQUAL(1, fake_hal_bluetooth_disconnect_count());
    CHECK_EQUAL(1, fake_hal_bluetooth_scan_count());
}

static void the_state_line_shows_the_pairing_result(void)
{
    Screen *screen = open_with_two_devices();
    HalBluetoothStatus *status = fake_hal_bluetooth();
    status->state = HAL_BLUETOOTH_READY;
    status->version = '5';
    status->pairing = HAL_BLUETOOTH_PAIRING_FAILED;
    screen->update(screen->context, START_MS + 200);
    CHECK(line_is(0, "Ready · v5 · Pairing failed"));
    status->state = HAL_BLUETOOTH_CONNECTING;
    status->pairing = HAL_BLUETOOTH_PAIRED;
    screen->update(screen->context, START_MS + 300);
    CHECK(line_is(0, "Connecting · v5 · Paired"));
}

static void the_connected_device_is_ticked(void)
{
    Screen *screen = open_with_two_devices();
    HalBluetoothStatus *status = fake_hal_bluetooth();
    status->state = HAL_BLUETOOTH_CONNECTED;
    status->connected = status->devices[1];
    screen->update(screen->context, START_MS + 200);
    CHECK(line_is(0, "Connected · Kitchen"));
    CHECK(line_is(4, "2. Kitchen ✓"));
    CHECK(line_is(3, "› 1. WH-1000XM4"));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(entering_powers_the_module_on),
        TEST_CASE(a_ready_module_starts_one_scan),
        TEST_CASE(play_scans_again),
        TEST_CASE(back_powers_the_module_off_and_leaves),
        TEST_CASE(the_state_line_shows_the_version_and_link_status),
        TEST_CASE(no_reply_is_reported),
        TEST_CASE(the_link_line_counts_bytes_frames_and_overruns),
        TEST_CASE(the_last_bytes_show_in_hex),
        TEST_CASE(nothing_received_says_so),
        TEST_CASE(found_devices_are_listed),
        TEST_CASE(volume_keys_move_the_selection_and_wrap),
        TEST_CASE(play_pairs_the_selected_device),
        TEST_CASE(holding_play_scans_again),
        TEST_CASE(holding_play_while_connected_disconnects),
        TEST_CASE(the_state_line_shows_the_pairing_result),
        TEST_CASE(the_connected_device_is_ticked),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

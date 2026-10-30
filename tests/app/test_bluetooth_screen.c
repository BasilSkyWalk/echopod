#include <string.h>

#include "bluetooth_link.h"
#include "bluetooth_store.h"
#include "check.h"
#include "default_theme.h"
#include "fake_hal.h"
#include "screens/bluetooth_screen.h"

enum {
    START_MS = 1000
};

static const HalBluetoothDevice BUDS = {.address = {0x5C, 0x17, 0xCF, 1, 2, 3},
                                        .name = "OnePlus Buds 3"};

static BluetoothLink link;
static Toast toast;
static BluetoothScreen bluetooth;

static Screen *open_screen(bool enabled, const HalBluetoothDevice *saved, bool details)
{
    fake_hal_reset();
    BluetoothSaved stored = {.enabled = enabled, .count = saved != NULL ? 1 : 0};
    if (saved != NULL) {
        stored.devices[0] = *saved;
    }
    bluetooth_store_save(&stored);
    toast = (Toast){0};
    bluetooth_link_start(&link, &toast, START_MS);
    Screen *screen = bluetooth_screen_init(&bluetooth, &link, &DEFAULT_THEME);
    ScreenAction open =
        details ? bluetooth_screen_open_details(&bluetooth) : bluetooth_screen_open(&bluetooth);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, open.type);
    CHECK(open.target == screen);
    screen->enter(screen->context, START_MS);
    return screen;
}

static void press(Screen *screen, Key key, KeyGesture gesture, uint32_t now_ms)
{
    screen->handle_key(screen->context, (KeyEvent){.key = key, .gesture = gesture}, now_ms);
}

static void tap(Screen *screen, Key key, uint32_t now_ms)
{
    press(screen, key, KEY_GESTURE_PRESS, now_ms);
}

static bool row_is(int index, const char *label, const char *value)
{
    return strcmp(label, bluetooth_screen_row(&bluetooth, index)) == 0 &&
           strcmp(value, bluetooth_screen_row_value(&bluetooth, index)) == 0;
}

static bool line_is(int index, const char *text)
{
    return strcmp(text, bluetooth_screen_line(&bluetooth, index)) == 0;
}

static void find_two_devices(void)
{
    HalBluetoothStatus *status = fake_hal_bluetooth();
    status->state = HAL_BLUETOOTH_SCANNING;
    status->device_count = 2;
    status->devices[0] = BUDS;
    strcpy(status->devices[1].name, "Kitchen");
    status->devices[1].address[0] = 0x22;
}

static void off_shows_only_the_switch(void)
{
    Screen *screen = open_screen(false, NULL, false);
    screen->update(screen->context, START_MS + 100);
    CHECK_EQUAL(1, bluetooth_screen_row_count(&bluetooth));
    CHECK(row_is(0, "Bluetooth", "Off"));
    CHECK(line_is(BLUETOOTH_SCREEN_HINT_LINE, "Play turns Bluetooth on"));
    CHECK(!fake_hal_bluetooth_powered());
}

static void play_on_the_switch_turns_bluetooth_on_and_off(void)
{
    Screen *screen = open_screen(false, NULL, false);
    tap(screen, KEY_PLAY, START_MS + 100);
    CHECK(fake_hal_bluetooth_powered());
    CHECK(row_is(0, "Bluetooth", "On"));
    tap(screen, KEY_PLAY, START_MS + 200);
    CHECK(!fake_hal_bluetooth_powered());
    CHECK(row_is(0, "Bluetooth", "Off"));
}

static void step_link(uint32_t now_ms)
{
    static Player idle;
    bluetooth_link_step(&link, &idle, now_ms);
}

static void a_ready_module_searches_while_the_screen_is_open(void)
{
    Screen *screen = open_screen(true, NULL, false);
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_READY;
    step_link(START_MS + 100);
    CHECK_EQUAL(1, fake_hal_bluetooth_scan_count());
    screen->update(screen->context, START_MS + 100);
    CHECK(row_is(1, "Pair new device", "Searching…"));
    tap(screen, KEY_VOLUME_UP, START_MS + 200);
    CHECK(line_is(BLUETOOTH_SCREEN_HINT_LINE, "Searching… put headphones in pairing mode"));
}

static void found_devices_are_listed_below_the_saved_one(void)
{
    Screen *screen = open_screen(true, &BUDS, false);
    find_two_devices();
    screen->update(screen->context, START_MS + 100);
    CHECK_EQUAL(4, bluetooth_screen_row_count(&bluetooth));
    CHECK(row_is(0, "Bluetooth", "On"));
    CHECK(row_is(1, "OnePlus Buds 3", "Nearby"));
    CHECK(row_is(2, "Pair new device", "Searching…"));
    CHECK(row_is(3, "Kitchen", ""));
}

static void play_on_a_found_device_pairs_with_it(void)
{
    Screen *screen = open_screen(true, NULL, false);
    find_two_devices();
    screen->update(screen->context, START_MS + 100);
    CHECK(row_is(3, "Kitchen", ""));
    tap(screen, KEY_VOLUME_UP, START_MS + 200);
    tap(screen, KEY_VOLUME_UP, START_MS + 300);
    tap(screen, KEY_VOLUME_UP, START_MS + 400);
    CHECK(line_is(BLUETOOTH_SCREEN_HINT_LINE, "Play pairs with this device"));
    tap(screen, KEY_PLAY, START_MS + 500);
    CHECK(row_is(3, "Kitchen", "Connecting…"));
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_READY;
    step_link(START_MS + 600);
    CHECK_EQUAL(1, fake_hal_bluetooth_connect_count());
    CHECK(strcmp("Kitchen", fake_hal_bluetooth_connect_target()->name) == 0);
    fake_hal_bluetooth()->pairing = HAL_BLUETOOTH_PAIRING_FAILED;
    step_link(START_MS + 700);
    screen->update(screen->context, START_MS + 700);
    CHECK(row_is(3, "Kitchen", "Didn't connect"));
}

static void the_connected_device_shows_its_state(void)
{
    Screen *screen = open_screen(true, &BUDS, false);
    find_two_devices();
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_CONNECTED;
    fake_hal_bluetooth()->connected = BUDS;
    step_link(START_MS + 100);
    screen->update(screen->context, START_MS + 100);
    CHECK_EQUAL(3, bluetooth_screen_row_count(&bluetooth));
    CHECK(row_is(1, "OnePlus Buds 3", "Connected"));
    CHECK(row_is(2, "Pair new device", ""));
    tap(screen, KEY_VOLUME_UP, START_MS + 200);
    CHECK(line_is(BLUETOOTH_SCREEN_HINT_LINE, "Play disconnects · hold Play forgets"));
    tap(screen, KEY_PLAY, START_MS + 300);
    CHECK_EQUAL(1, fake_hal_bluetooth_disconnect_count());
}

static void pair_new_device_disconnects_and_searches(void)
{
    Screen *screen = open_screen(true, &BUDS, false);
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_CONNECTED;
    fake_hal_bluetooth()->connected = BUDS;
    step_link(START_MS + 100);
    screen->update(screen->context, START_MS + 100);
    tap(screen, KEY_VOLUME_UP, START_MS + 200);
    tap(screen, KEY_VOLUME_UP, START_MS + 300);
    CHECK(line_is(BLUETOOTH_SCREEN_HINT_LINE, "Play searches for new headphones"));
    tap(screen, KEY_PLAY, START_MS + 400);
    CHECK_EQUAL(1, fake_hal_bluetooth_disconnect_count());
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_READY;
    step_link(START_MS + 500);
    CHECK_EQUAL(1, fake_hal_bluetooth_scan_count());
}

static void holding_play_on_the_saved_device_forgets_it(void)
{
    Screen *screen = open_screen(true, &BUDS, false);
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_CONNECTED;
    fake_hal_bluetooth()->connected = BUDS;
    step_link(START_MS + 100);
    screen->update(screen->context, START_MS + 100);
    tap(screen, KEY_VOLUME_UP, START_MS + 200);
    press(screen, KEY_PLAY, KEY_GESTURE_LONG_PRESS, START_MS + 300);
    CHECK_EQUAL(1, fake_hal_bluetooth_disconnect_count());
    CHECK_EQUAL(2, bluetooth_screen_row_count(&bluetooth));
    BluetoothSaved saved;
    bluetooth_store_load(&saved);
    CHECK_EQUAL(0, saved.count);
}

static void play_on_a_disconnected_saved_device_reconnects(void)
{
    Screen *screen = open_screen(true, &BUDS, false);
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_READY;
    screen->update(screen->context, START_MS + 100);
    tap(screen, KEY_VOLUME_UP, START_MS + 200);
    CHECK(line_is(BLUETOOTH_SCREEN_HINT_LINE, "Play connects · hold Play forgets"));
    tap(screen, KEY_PLAY, START_MS + 300);
    CHECK(row_is(1, "OnePlus Buds 3", "Connecting…"));
    step_link(START_MS + 400);
    CHECK_EQUAL(1, fake_hal_bluetooth_connect_count());
    CHECK(strcmp("OnePlus Buds 3", fake_hal_bluetooth_connect_target()->name) == 0);
}

static void leaving_keeps_the_module_on_and_stops_browsing(void)
{
    Screen *screen = open_screen(true, NULL, false);
    CHECK(link.browsing);
    tap(screen, KEY_BACK, START_MS + 100);
    CHECK(!link.browsing);
    CHECK(fake_hal_bluetooth_powered());
}

static void details_show_the_link(void)
{
    Screen *screen = open_screen(true, NULL, true);
    HalBluetoothStatus *status = fake_hal_bluetooth();
    status->state = HAL_BLUETOOTH_READY;
    status->version = '5';
    status->link.received = 123;
    status->link.frames = 9;
    status->link.overruns = 2;
    status->link.fifo_peak = 7;
    status->link.fifo_depth = 16;
    status->link.events[0][0] = 'C';
    status->link.events[0][1] = '.';
    status->link.event_count = 1;
    screen->update(screen->context, START_MS + 100);
    CHECK(line_is(1, "Ready · v5"));
    CHECK(line_is(2, "RX 123 · 9 frames · OE 2 · FIFO 7/16"));
    CHECK(line_is(3, "Msgs CC."));
    CHECK(line_is(4, ""));
}

static void details_spread_a_long_message_log_over_two_lines(void)
{
    static const char LETTERS[] = "VSKQLHTSPQOX";
    Screen *screen = open_screen(true, NULL, true);
    HalBluetoothStatus *status = fake_hal_bluetooth();
    for (int index = 0; index < HAL_BLUETOOTH_EVENT_CAPACITY; index++) {
        status->link.events[index][0] = index % 3 == 0 ? '>' : 'C';
        status->link.events[index][1] = LETTERS[index];
        status->link.gaps[index] = (uint8_t)index;
    }
    status->link.event_count = HAL_BLUETOOTH_EVENT_CAPACITY;
    screen->update(screen->context, START_MS + 100);
    CHECK(line_is(3, "Msgs >V CCS+1 CCK+2 >Q+3 CCL+4 CCH+5"));
    CHECK(line_is(4, " >T+6 CCS+7 CCP+8 >Q+9 CCO+10 CCX+11"));
}

static void pair_new_device_shows_searching_while_the_module_restarts(void)
{
    Screen *screen = open_screen(true, &BUDS, false);
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_READY;
    step_link(START_MS + 100);
    CHECK_EQUAL(1, fake_hal_bluetooth_connect_count());
    fake_hal_bluetooth()->pairing = HAL_BLUETOOTH_PAIRING_FAILED;
    step_link(START_MS + 200);
    CHECK(!fake_hal_bluetooth_powered());
    screen->update(screen->context, START_MS + 200);
    CHECK(row_is(1, "OnePlus Buds 3", ""));
    CHECK(row_is(2, "Pair new device", "Searching…"));
    CHECK(strcmp("Searching…", bluetooth_screen_summary(&bluetooth)) == 0);
    step_link(START_MS + 200 + BLUETOOTH_LINK_RESTART_MS);
    CHECK(fake_hal_bluetooth_powered());
    screen->update(screen->context, START_MS + 200 + BLUETOOTH_LINK_RESTART_MS);
    CHECK(row_is(1, "OnePlus Buds 3", ""));
    CHECK(row_is(2, "Pair new device", "Searching…"));
}

static void the_settings_row_summarises_the_link(void)
{
    open_screen(false, NULL, false);
    CHECK(strcmp("Off", bluetooth_screen_summary(&bluetooth)) == 0);
    bluetooth_link_set_enabled(&link, true, START_MS + 100);
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_CONNECTED;
    fake_hal_bluetooth()->connected = BUDS;
    step_link(START_MS + 200);
    CHECK(strcmp("OnePlus Buds 3", bluetooth_screen_summary(&bluetooth)) == 0);
}

static void a_connection_is_saved_once(void)
{
    Screen *screen = open_screen(true, NULL, false);
    fake_hal_bluetooth()->state = HAL_BLUETOOTH_CONNECTED;
    fake_hal_bluetooth()->connected = BUDS;
    step_link(START_MS + 100);
    int writes = fake_hal_write_count();
    step_link(START_MS + 200);
    screen->update(screen->context, START_MS + 200);
    CHECK_EQUAL(writes, fake_hal_write_count());
}

static void a_damaged_store_is_ignored(void)
{
    fake_hal_reset();
    static const uint8_t junk[] = {'X', 'Y', 1, 1, 2, 3, 4, 5, 6};
    fake_hal_add_file(BLUETOOTH_STORE_PATH, junk, sizeof junk);
    BluetoothSaved saved;
    bluetooth_store_load(&saved);
    CHECK(!saved.enabled);
    CHECK_EQUAL(0, saved.count);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(off_shows_only_the_switch),
        TEST_CASE(play_on_the_switch_turns_bluetooth_on_and_off),
        TEST_CASE(a_ready_module_searches_while_the_screen_is_open),
        TEST_CASE(found_devices_are_listed_below_the_saved_one),
        TEST_CASE(play_on_a_found_device_pairs_with_it),
        TEST_CASE(the_connected_device_shows_its_state),
        TEST_CASE(pair_new_device_disconnects_and_searches),
        TEST_CASE(holding_play_on_the_saved_device_forgets_it),
        TEST_CASE(play_on_a_disconnected_saved_device_reconnects),
        TEST_CASE(leaving_keeps_the_module_on_and_stops_browsing),
        TEST_CASE(details_show_the_link),
        TEST_CASE(details_spread_a_long_message_log_over_two_lines),
        TEST_CASE(pair_new_device_shows_searching_while_the_module_restarts),
        TEST_CASE(the_settings_row_summarises_the_link),
        TEST_CASE(a_connection_is_saved_once),
        TEST_CASE(a_damaged_store_is_ignored),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

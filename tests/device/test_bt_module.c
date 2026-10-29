#include <string.h>

#include "check.h"
#include "device/bt_module.h"

enum {
    START_MS = 1000
};

static BtModule module;

static void receive_text(const char *text, uint32_t now_ms)
{
    for (size_t index = 0; text[index] != '\0'; index++) {
        bt_module_receive(&module, (uint8_t)text[index], now_ms);
    }
}

static void receive_bytes(const uint8_t *bytes, size_t length, uint32_t now_ms)
{
    for (size_t index = 0; index < length; index++) {
        bt_module_receive(&module, bytes[index], now_ms);
    }
}

static bool poll(uint32_t now_ms, BtModuleFrame *frame)
{
    return bt_module_poll(&module, now_ms, frame);
}

static void settle(uint32_t now_ms)
{
    BtModuleFrame frame;
    poll(now_ms + BT_MODULE_SILENCE_MS, &frame);
}

static void power_on_ready(void)
{
    bt_module_power_on(&module, START_MS);
    receive_text("CV15", START_MS + 200);
    settle(START_MS + 200);
}

static bool frame_is(const BtModuleFrame *frame, const char *text)
{
    size_t length = strlen(text) + 1;
    return frame->length == length && memcmp(frame->bytes, text, length) == 0;
}

static void a_powered_module_waits_for_its_ready_report(void)
{
    bt_module_power_on(&module, START_MS);
    CHECK_EQUAL(HAL_BLUETOOTH_STARTING, module.status.state);
    receive_text("CV15", START_MS + 300);
    CHECK_EQUAL(HAL_BLUETOOTH_STARTING, module.status.state);
    BtModuleFrame frame;
    CHECK(!poll(START_MS + 300, &frame));
    CHECK_EQUAL(HAL_BLUETOOTH_READY, module.status.state);
    CHECK_EQUAL('5', module.status.version);
}

static void a_frame_split_across_reads_waits_for_the_rest(void)
{
    BtModuleFrame frame;
    bt_module_power_on(&module, START_MS);
    receive_text("CV", START_MS + 300);
    poll(START_MS + 300, &frame);
    receive_text("15", START_MS + 301);
    poll(START_MS + 301, &frame);
    CHECK_EQUAL(HAL_BLUETOOTH_READY, module.status.state);
}

static void a_cut_off_frame_is_dropped_after_a_pause(void)
{
    BtModuleFrame frame;
    bt_module_power_on(&module, START_MS);
    receive_text("CV", START_MS + 300);
    poll(START_MS + 300 + BT_MODULE_SILENCE_MS, &frame);
    receive_text("15", START_MS + 400);
    poll(START_MS + 400 + BT_MODULE_SILENCE_MS, &frame);
    CHECK_EQUAL(HAL_BLUETOOTH_STARTING, module.status.state);
}

static void a_long_burst_is_read_in_full(void)
{
    BtModuleFrame frame;
    power_on_ready();
    bt_module_scan(&module, START_MS + 500, &frame);
    for (int round = 0; round < 40; round++) {
        receive_text("CKKK", START_MS + 600);
    }
    receive_text("CSEN", START_MS + 600);
    poll(START_MS + 600, &frame);
    CHECK_EQUAL(HAL_BLUETOOTH_READY, module.status.state);
    CHECK_EQUAL(42u, module.status.link.frames);
}

static void no_ready_report_within_five_seconds_means_no_reply(void)
{
    BtModuleFrame frame;
    bt_module_power_on(&module, START_MS);
    poll(START_MS + BT_MODULE_READY_TIMEOUT_MS - 1, &frame);
    CHECK_EQUAL(HAL_BLUETOOTH_STARTING, module.status.state);
    poll(START_MS + BT_MODULE_READY_TIMEOUT_MS, &frame);
    CHECK_EQUAL(HAL_BLUETOOTH_NO_REPLY, module.status.state);
}

static void noise_before_the_ready_report_is_skipped(void)
{
    static const uint8_t NOISE[] = {0x00, 0xFF, 0x43, 0x00};
    bt_module_power_on(&module, START_MS);
    receive_bytes(NOISE, sizeof NOISE, START_MS + 100);
    receive_text("CV10", START_MS + 100);
    settle(START_MS + 100);
    CHECK_EQUAL(HAL_BLUETOOTH_READY, module.status.state);
    CHECK_EQUAL('0', module.status.version);
}

static void a_scan_sends_cs00(void)
{
    power_on_ready();
    BtModuleFrame frame;
    CHECK(bt_module_scan(&module, START_MS + 500, &frame));
    CHECK(frame_is(&frame, "CS00"));
    CHECK_EQUAL(HAL_BLUETOOTH_SCANNING, module.status.state);
    CHECK_EQUAL(0u, module.status.device_count);
}

static void a_scan_needs_a_ready_module(void)
{
    BtModuleFrame frame;
    bt_module_power_on(&module, START_MS);
    CHECK(!bt_module_scan(&module, START_MS + 10, &frame));
    CHECK_EQUAL(HAL_BLUETOOTH_STARTING, module.status.state);
}

static void receive_device(const uint8_t address[6], const char *name, uint32_t now_ms)
{
    size_t name_length = strlen(name);
    size_t payload = 6 + name_length;
    uint8_t bytes[64] = {'C', 'S', (uint8_t)('0' + payload / 10), (uint8_t)('0' + payload % 10)};
    memcpy(bytes + 4, address, 6);
    memcpy(bytes + 10, name, name_length);
    receive_bytes(bytes, 4 + payload, now_ms);
}

static const uint8_t PHONES[6] = {0x11, 0x22, 0x43, 0x44, 0x55, 0x66};
static const uint8_t SPEAKER[6] = {0xA0, 0xB1, 0xC2, 0xD3, 0xE4, 0xF5};

static void scan_results_fill_the_device_list(void)
{
    BtModuleFrame frame;
    power_on_ready();
    bt_module_scan(&module, START_MS + 500, &frame);
    receive_device(PHONES, "WH-1000XM4", START_MS + 1500);
    receive_device(SPEAKER, "Kitchen", START_MS + 1500);
    settle(START_MS + 1500);
    CHECK_EQUAL(2u, module.status.device_count);
    CHECK(memcmp(PHONES, module.status.devices[0].address, 6) == 0);
    CHECK(strcmp("WH-1000XM4", module.status.devices[0].name) == 0);
    CHECK(strcmp("Kitchen", module.status.devices[1].name) == 0);
    CHECK_EQUAL(HAL_BLUETOOTH_SCANNING, module.status.state);
}

static void csen_ends_the_scan(void)
{
    BtModuleFrame frame;
    power_on_ready();
    bt_module_scan(&module, START_MS + 500, &frame);
    receive_device(PHONES, "WH-1000XM4", START_MS + 1500);
    receive_text("CSEN", START_MS + 1500);
    settle(START_MS + 1500);
    CHECK_EQUAL(1u, module.status.device_count);
    CHECK_EQUAL(HAL_BLUETOOTH_READY, module.status.state);
}

static void repeated_and_unnamed_devices_are_dropped(void)
{
    BtModuleFrame frame;
    power_on_ready();
    bt_module_scan(&module, START_MS + 500, &frame);
    receive_device(PHONES, "WH-1000XM4", START_MS + 1500);
    receive_device(PHONES, "WH-1000XM4", START_MS + 1500);
    receive_device(SPEAKER, "", START_MS + 1500);
    settle(START_MS + 1500);
    CHECK_EQUAL(1u, module.status.device_count);
}

static void the_list_holds_six_devices(void)
{
    BtModuleFrame frame;
    power_on_ready();
    bt_module_scan(&module, START_MS + 500, &frame);
    for (uint8_t index = 0; index < 8; index++) {
        uint8_t address[6] = {1, 2, 3, 4, 5, index};
        receive_device(address, "Device", START_MS + 1500);
    }
    settle(START_MS + 1500);
    CHECK_EQUAL(HAL_BLUETOOTH_DEVICE_CAPACITY, module.status.device_count);
}

static void names_up_to_31_bytes_fit(void)
{
    BtModuleFrame frame;
    power_on_ready();
    bt_module_scan(&module, START_MS + 500, &frame);
    receive_device(PHONES, "A name that is exactly 31 bytes", START_MS + 1500);
    settle(START_MS + 1500);
    CHECK_EQUAL(1u, module.status.device_count);
    CHECK(strcmp("A name that is exactly 31 bytes", module.status.devices[0].name) == 0);
}

static void an_oversized_scan_frame_ends_the_scan(void)
{
    BtModuleFrame frame;
    power_on_ready();
    bt_module_scan(&module, START_MS + 500, &frame);
    receive_text("CS38", START_MS + 1500);
    settle(START_MS + 1500);
    CHECK_EQUAL(HAL_BLUETOOTH_READY, module.status.state);
}

static void an_unanswered_scan_is_sent_once_more_after_four_seconds(void)
{
    BtModuleFrame frame;
    power_on_ready();
    uint32_t scan_ms = START_MS + 500;
    bt_module_scan(&module, scan_ms, &frame);
    CHECK(!poll(scan_ms + BT_MODULE_SCAN_RETRY_MS - 1, &frame));
    CHECK(poll(scan_ms + BT_MODULE_SCAN_RETRY_MS, &frame));
    CHECK(frame_is(&frame, "CS00"));
    CHECK(!poll(scan_ms + 2 * BT_MODULE_SCAN_RETRY_MS, &frame));
}

static void an_answered_scan_is_not_repeated(void)
{
    BtModuleFrame frame;
    power_on_ready();
    uint32_t scan_ms = START_MS + 500;
    bt_module_scan(&module, scan_ms, &frame);
    receive_text("CKKK", scan_ms + 100);
    settle(scan_ms + 100);
    CHECK(!poll(scan_ms + BT_MODULE_SCAN_RETRY_MS, &frame));
}

static void a_scan_stops_with_cp00_after_thirty_seconds(void)
{
    BtModuleFrame frame;
    power_on_ready();
    uint32_t scan_ms = START_MS + 500;
    bt_module_scan(&module, scan_ms, &frame);
    receive_text("CKKK", scan_ms + 100);
    settle(scan_ms + 100);
    CHECK(poll(scan_ms + BT_MODULE_SCAN_LIMIT_MS, &frame));
    CHECK(frame_is(&frame, "CP00"));
    CHECK_EQUAL(HAL_BLUETOOTH_READY, module.status.state);
}

static void a_scan_pauses_the_status_poll(void)
{
    BtModuleFrame frame;
    power_on_ready();
    uint32_t scan_ms = START_MS + 500;
    bt_module_scan(&module, scan_ms, &frame);
    receive_text("CKKK", scan_ms + 100);
    settle(scan_ms + 100);
    CHECK(!poll(scan_ms + BT_MODULE_SCAN_LIMIT_MS - 1, &frame));
}

static void a_ready_module_is_asked_for_its_status_every_three_seconds(void)
{
    BtModuleFrame frame;
    power_on_ready();
    uint32_t ready_ms = START_MS + 200 + BT_MODULE_SILENCE_MS;
    CHECK(!poll(ready_ms + BT_MODULE_QUERY_PERIOD_MS - 1, &frame));
    CHECK(poll(ready_ms + BT_MODULE_QUERY_PERIOD_MS, &frame));
    CHECK(frame_is(&frame, "CQ00"));
    CHECK(!poll(ready_ms + BT_MODULE_QUERY_PERIOD_MS + 1, &frame));
}

static void status_replies_set_the_link_letter(void)
{
    power_on_ready();
    receive_text("CQI", START_MS + 400);
    settle(START_MS + 400);
    CHECK_EQUAL('I', module.status.link_status);
    receive_text("CQS", START_MS + 500);
    settle(START_MS + 500);
    CHECK_EQUAL('S', module.status.link_status);
}

static void a_connected_reply_skips_its_payload(void)
{
    static const uint8_t CONNECTED[] = {'C',  'Q',  'O',  '0',  '8', 0x43, 0x56,
                                        0x31, 0x39, 0x00, 0x01, 'H', 'i'};
    power_on_ready();
    receive_bytes(CONNECTED, sizeof CONNECTED, START_MS + 400);
    receive_text("CQI", START_MS + 400);
    settle(START_MS + 400);
    CHECK_EQUAL('I', module.status.link_status);
    CHECK_EQUAL('5', module.status.version);
}

static void the_last_received_bytes_are_kept(void)
{
    bt_module_power_on(&module, START_MS);
    receive_text("CV15CKKKCQI0123", START_MS + 100);
    CHECK_EQUAL(HAL_BLUETOOTH_RECENT_CAPACITY, module.status.link.recent_length);
    CHECK(memcmp("5CKKKCQI0123", module.status.link.recent, HAL_BLUETOOTH_RECENT_CAPACITY) == 0);
    CHECK_EQUAL(15u, module.status.link.received);
}

static void frames_are_counted(void)
{
    power_on_ready();
    receive_text("CKKKCQI", START_MS + 400);
    settle(START_MS + 400);
    CHECK_EQUAL(3u, module.status.link.frames);
}

static void powering_off_forgets_the_session(void)
{
    BtModuleFrame frame;
    power_on_ready();
    bt_module_scan(&module, START_MS + 500, &frame);
    receive_device(PHONES, "WH-1000XM4", START_MS + 1500);
    settle(START_MS + 1500);
    bt_module_power_off(&module);
    CHECK_EQUAL(HAL_BLUETOOTH_OFF, module.status.state);
    CHECK_EQUAL(0u, module.status.device_count);
    CHECK(!poll(START_MS + 60000, &frame));
}

static void found_phones(void)
{
    BtModuleFrame frame;
    power_on_ready();
    bt_module_scan(&module, START_MS + 500, &frame);
    receive_device(PHONES, "WH-1000XM4", START_MS + 1500);
    receive_device(SPEAKER, "Kitchen", START_MS + 1500);
    receive_text("CSEN", START_MS + 1500);
    settle(START_MS + 1500);
}

static bool frame_is_connect(const BtModuleFrame *frame, const char *command,
                             const uint8_t address[6])
{
    return frame->length == 10 && memcmp(frame->bytes, command, 4) == 0 &&
           memcmp(frame->bytes + 4, address, 6) == 0;
}

enum {
    CONNECT_MS = START_MS + 2000
};

static void connecting_sends_cl06_with_the_picked_address(void)
{
    BtModuleFrame frame;
    found_phones();
    CHECK(bt_module_connect(&module, &module.status.devices[1], CONNECT_MS, &frame));
    CHECK(frame_is_connect(&frame, "CL06", SPEAKER));
    CHECK_EQUAL(HAL_BLUETOOTH_CONNECTING, module.status.state);
    CHECK_EQUAL(HAL_BLUETOOTH_PAIRING_NONE, module.status.pairing);
}

static void connecting_needs_a_ready_module(void)
{
    BtModuleFrame frame;
    HalBluetoothDevice device = {.name = "WH-1000XM4"};
    memcpy(device.address, PHONES, sizeof device.address);
    bt_module_power_on(&module, START_MS);
    CHECK(!bt_module_connect(&module, &device, START_MS + 100, &frame));
    CHECK_EQUAL(HAL_BLUETOOTH_STARTING, module.status.state);
}

static void a_remembered_device_connects_without_a_scan(void)
{
    BtModuleFrame frame;
    HalBluetoothDevice device = {.name = "WH-1000XM4"};
    memcpy(device.address, PHONES, sizeof device.address);
    power_on_ready();
    CHECK(bt_module_connect(&module, &device, CONNECT_MS, &frame));
    CHECK(frame_is_connect(&frame, "CL06", PHONES));
    CHECK_EQUAL(HAL_BLUETOOTH_CONNECTING, module.status.state);
    CHECK(bt_module_poll(&module, CONNECT_MS + BT_MODULE_CONNECT_RETRY_MS, &frame));
    CHECK(frame_is_connect(&frame, "CH06", PHONES));
}

static void connecting_works_while_still_scanning(void)
{
    BtModuleFrame frame;
    power_on_ready();
    bt_module_scan(&module, START_MS + 500, &frame);
    receive_device(PHONES, "WH-1000XM4", START_MS + 1500);
    settle(START_MS + 1500);
    CHECK(bt_module_connect(&module, &module.status.devices[0], CONNECT_MS, &frame));
    CHECK(frame_is_connect(&frame, "CL06", PHONES));
}

static void clok_reports_paired(void)
{
    BtModuleFrame frame;
    found_phones();
    bt_module_connect(&module, &module.status.devices[0], CONNECT_MS, &frame);
    receive_text("CLOK", CONNECT_MS + 1000);
    settle(CONNECT_MS + 1000);
    CHECK_EQUAL(HAL_BLUETOOTH_PAIRED, module.status.pairing);
    CHECK_EQUAL(HAL_BLUETOOTH_READY, module.status.state);
}

static void cler_and_cher_report_a_failed_pairing(void)
{
    static const char *const FAILURES[] = {"CLER", "CHER"};
    for (int index = 0; index < 2; index++) {
        BtModuleFrame frame;
        found_phones();
        bt_module_connect(&module, &module.status.devices[0], CONNECT_MS, &frame);
        receive_text(FAILURES[index], CONNECT_MS + 1000);
        settle(CONNECT_MS + 1000);
        CHECK_EQUAL(HAL_BLUETOOTH_PAIRING_FAILED, module.status.pairing);
        CHECK_EQUAL(HAL_BLUETOOTH_READY, module.status.state);
    }
}

static void an_unanswered_connect_retries_once_with_ch06(void)
{
    BtModuleFrame frame;
    found_phones();
    bt_module_connect(&module, &module.status.devices[0], CONNECT_MS, &frame);
    CHECK(!poll(CONNECT_MS + BT_MODULE_CONNECT_RETRY_MS - 1, &frame));
    CHECK(poll(CONNECT_MS + BT_MODULE_CONNECT_RETRY_MS, &frame));
    CHECK(frame_is_connect(&frame, "CH06", PHONES));
    CHECK(!poll(CONNECT_MS + 2 * BT_MODULE_CONNECT_RETRY_MS, &frame));
}

static void a_connect_without_a_result_is_cancelled_after_thirty_seconds(void)
{
    BtModuleFrame frame;
    found_phones();
    bt_module_connect(&module, &module.status.devices[0], CONNECT_MS, &frame);
    receive_text("CKKK", CONNECT_MS + 100);
    settle(CONNECT_MS + 100);
    CHECK(!poll(CONNECT_MS + BT_MODULE_CONNECT_LIMIT_MS - 1, &frame));
    CHECK(poll(CONNECT_MS + BT_MODULE_CONNECT_LIMIT_MS, &frame));
    CHECK(frame_is(&frame, "CT00"));
    CHECK_EQUAL(HAL_BLUETOOTH_READY, module.status.state);
    CHECK_EQUAL(HAL_BLUETOOTH_PAIRING_FAILED, module.status.pairing);
}

static void receive_connected(const uint8_t address[6], const char *name, uint32_t now_ms)
{
    size_t name_length = strlen(name);
    size_t payload = 6 + name_length;
    uint8_t bytes[128] = {'C', 'Q', 'O', (uint8_t)('0' + payload / 10),
                          (uint8_t)('0' + payload % 10)};
    memcpy(bytes + 5, address, 6);
    memcpy(bytes + 11, name, name_length);
    receive_bytes(bytes, 5 + payload, now_ms);
}

static void cqo_reports_the_connected_device(void)
{
    BtModuleFrame frame;
    found_phones();
    bt_module_connect(&module, &module.status.devices[0], CONNECT_MS, &frame);
    receive_text("CLOK", CONNECT_MS + 1000);
    receive_connected(PHONES, "WH-1000XM4", CONNECT_MS + 1000);
    settle(CONNECT_MS + 1000);
    CHECK_EQUAL(HAL_BLUETOOTH_CONNECTED, module.status.state);
    CHECK_EQUAL('O', module.status.link_status);
    CHECK(memcmp(PHONES, module.status.connected.address, 6) == 0);
    CHECK(strcmp("WH-1000XM4", module.status.connected.name) == 0);
}

static void a_long_connected_name_is_cut(void)
{
    power_on_ready();
    receive_connected(PHONES, "A device name that is longer than thirty-one bytes", START_MS + 400);
    settle(START_MS + 400);
    CHECK_EQUAL(HAL_BLUETOOTH_CONNECTED, module.status.state);
    CHECK(strcmp("A device name that is longer th", module.status.connected.name) == 0);
}

static void chok_connects_the_picked_device(void)
{
    BtModuleFrame frame;
    found_phones();
    bt_module_connect(&module, &module.status.devices[1], CONNECT_MS, &frame);
    receive_text("CHOK", CONNECT_MS + 1000);
    settle(CONNECT_MS + 1000);
    CHECK_EQUAL(HAL_BLUETOOTH_CONNECTED, module.status.state);
    CHECK_EQUAL(HAL_BLUETOOTH_PAIRED, module.status.pairing);
    CHECK(strcmp("Kitchen", module.status.connected.name) == 0);
}

static void connected_for(uint32_t now_ms)
{
    power_on_ready();
    receive_connected(PHONES, "WH-1000XM4", now_ms);
    settle(now_ms);
}

static void a_connected_module_is_still_asked_for_its_status(void)
{
    BtModuleFrame frame;
    connected_for(START_MS + 400);
    uint32_t connected_ms = START_MS + 400 + BT_MODULE_SILENCE_MS;
    CHECK(!poll(connected_ms + BT_MODULE_QUERY_PERIOD_MS - 1, &frame));
    CHECK(poll(connected_ms + BT_MODULE_QUERY_PERIOD_MS, &frame));
    CHECK(frame_is(&frame, "CQ00"));
}

static void another_status_letter_means_the_link_dropped(void)
{
    connected_for(START_MS + 400);
    receive_text("CQI", START_MS + 500);
    settle(START_MS + 500);
    CHECK_EQUAL(HAL_BLUETOOTH_READY, module.status.state);
}

static void disconnecting_sends_ct00_and_ctok_confirms_it(void)
{
    BtModuleFrame frame;
    connected_for(START_MS + 400);
    CHECK(bt_module_disconnect(&module, START_MS + 500, &frame));
    CHECK(frame_is(&frame, "CT00"));
    receive_text("CTOK", START_MS + 600);
    settle(START_MS + 600);
    CHECK_EQUAL(HAL_BLUETOOTH_READY, module.status.state);
}

static void disconnecting_needs_a_link(void)
{
    BtModuleFrame frame;
    power_on_ready();
    CHECK(!bt_module_disconnect(&module, START_MS + 500, &frame));
}

static void a_disconnect_cancels_a_pending_connect(void)
{
    BtModuleFrame frame;
    found_phones();
    bt_module_connect(&module, &module.status.devices[0], CONNECT_MS, &frame);
    CHECK(bt_module_disconnect(&module, START_MS + 500, &frame));
    CHECK(frame_is(&frame, "CT00"));
    CHECK_EQUAL(HAL_BLUETOOTH_READY, module.status.state);
}

static bool frame_bytes_are(const BtModuleFrame *frame, const uint8_t *bytes, uint8_t length)
{
    return frame->length == length && memcmp(frame->bytes, bytes, length) == 0;
}

static void the_first_stream_start_sends_only_cy0_1e(void)
{
    static const uint8_t STREAM_ON[] = {'C', 'Y', '0', 0x1E, 0x00};
    BtModuleFrame frame;
    connected_for(START_MS + 400);
    CHECK(bt_module_set_stream(&module, true, START_MS + 500, &frame));
    CHECK(frame_bytes_are(&frame, STREAM_ON, sizeof STREAM_ON));
    CHECK(module.status.streaming);
    CHECK(!poll(START_MS + 500 + BT_MODULE_PLAY_NOTICE_DELAY_MS, &frame));
    CHECK(!poll(START_MS + 500 + 2 * BT_MODULE_PLAY_NOTICE_DELAY_MS, &frame));
}

static void resuming_after_a_stop_sends_the_play_notice(void)
{
    static const uint8_t STREAM_ON[] = {'C', 'Y', '0', 0x1E, 0x00};
    static const uint8_t PLAY_NOTICE[] = {'C', 0x86, '0', '0', 0x00};
    BtModuleFrame frame;
    connected_for(START_MS + 400);
    bt_module_set_stream(&module, true, START_MS + 500, &frame);
    bt_module_set_stream(&module, false, START_MS + 600, &frame);
    poll(START_MS + 600 + BT_MODULE_PLAY_NOTICE_DELAY_MS, &frame);
    CHECK(bt_module_set_stream(&module, true, START_MS + 900, &frame));
    CHECK(frame_bytes_are(&frame, STREAM_ON, sizeof STREAM_ON));
    CHECK(!poll(START_MS + 900 + BT_MODULE_PLAY_NOTICE_DELAY_MS - 1, &frame));
    CHECK(poll(START_MS + 900 + BT_MODULE_PLAY_NOTICE_DELAY_MS, &frame));
    CHECK(frame_bytes_are(&frame, PLAY_NOTICE, sizeof PLAY_NOTICE));
}

static void stopping_the_stream_sends_cy0_00_then_the_pause_notice(void)
{
    static const uint8_t STREAM_OFF[] = {'C', 'Y', '0', 0x00, 0x00};
    static const uint8_t PAUSE_NOTICE[] = {'C', 0x87, '0', '0', 0x00};
    BtModuleFrame frame;
    connected_for(START_MS + 400);
    bt_module_set_stream(&module, true, START_MS + 500, &frame);
    poll(START_MS + 700, &frame);
    CHECK(bt_module_set_stream(&module, false, START_MS + 800, &frame));
    CHECK(frame_bytes_are(&frame, STREAM_OFF, sizeof STREAM_OFF));
    CHECK(!module.status.streaming);
    CHECK(poll(START_MS + 800 + BT_MODULE_PLAY_NOTICE_DELAY_MS, &frame));
    CHECK(frame_bytes_are(&frame, PAUSE_NOTICE, sizeof PAUSE_NOTICE));
}

static void module_messages_are_kept_without_status_or_scan_replies(void)
{
    found_phones();
    receive_text("CKKK", START_MS + 1600);
    settle(START_MS + 1600);
    const HalBluetoothLink *link = &module.status.link;
    CHECK_EQUAL(2, link->event_count);
    CHECK_EQUAL('V', link->events[0][0]);
    CHECK_EQUAL('.', link->events[0][1]);
    CHECK_EQUAL('K', link->events[1][0]);
    CHECK_EQUAL('K', link->events[1][1]);
}

static void a_play_report_pauses_status_queries_for_fifty_seconds(void)
{
    BtModuleFrame frame;
    connected_for(START_MS + 400);
    uint32_t heard_ms = START_MS + 500 + BT_MODULE_SILENCE_MS;
    receive_text("CP", START_MS + 500);
    settle(START_MS + 500);
    CHECK(!poll(heard_ms + BT_MODULE_QUERY_PERIOD_MS * 2, &frame));
    CHECK(!poll(heard_ms + BT_MODULE_QUERY_PAUSE_MS - 1, &frame));
    CHECK(poll(heard_ms + BT_MODULE_QUERY_PAUSE_MS, &frame));
    CHECK(frame_is(&frame, "CQ00"));
}

static void a_play_report_after_clok_means_connected(void)
{
    BtModuleFrame frame;
    found_phones();
    bt_module_connect(&module, &module.status.devices[0], CONNECT_MS, &frame);
    receive_text("CLOK", CONNECT_MS + 1000);
    settle(CONNECT_MS + 1000);
    receive_text("CC", CONNECT_MS + 1200);
    settle(CONNECT_MS + 1200);
    CHECK_EQUAL(HAL_BLUETOOTH_CONNECTED, module.status.state);
    CHECK_EQUAL(0x11, module.status.connected.address[0]);
}

static void a_play_report_before_a_link_keeps_the_status_poll(void)
{
    BtModuleFrame frame;
    power_on_ready();
    uint32_t heard_ms = START_MS + 500 + BT_MODULE_SILENCE_MS;
    receive_text("CP", START_MS + 500);
    settle(START_MS + 500);
    CHECK_EQUAL(HAL_BLUETOOTH_READY, module.status.state);
    CHECK(poll(heard_ms + BT_MODULE_QUERY_PERIOD_MS, &frame));
    CHECK(frame_is(&frame, "CQ00"));
}

static void ctok_after_clok_clears_the_pairing(void)
{
    BtModuleFrame frame;
    found_phones();
    bt_module_connect(&module, &module.status.devices[0], CONNECT_MS, &frame);
    receive_text("CLOK", CONNECT_MS + 1000);
    settle(CONNECT_MS + 1000);
    receive_text("CTOK", CONNECT_MS + 2000);
    settle(CONNECT_MS + 2000);
    CHECK_EQUAL(HAL_BLUETOOTH_PAIRING_NONE, module.status.pairing);
    CHECK_EQUAL(HAL_BLUETOOTH_READY, module.status.state);
}

static void a_dropped_link_clears_the_pairing(void)
{
    connected_for(START_MS + 400);
    module.status.pairing = HAL_BLUETOOTH_PAIRED;
    receive_text("CQI", START_MS + 500);
    settle(START_MS + 500);
    CHECK_EQUAL(HAL_BLUETOOTH_PAIRING_NONE, module.status.pairing);
}

static void buds_controls_are_counted_without_sending_anything(void)
{
    BtModuleFrame frame;
    connected_for(START_MS + 400);
    bt_module_set_stream(&module, true, START_MS + 500, &frame);
    receive_text("CP", START_MS + 600);
    CHECK(!poll(START_MS + 600 + BT_MODULE_SILENCE_MS, &frame));
    receive_text("CC", START_MS + 700);
    receive_text("CA", START_MS + 700);
    receive_text("CB", START_MS + 700);
    receive_text("CB", START_MS + 700);
    CHECK(!poll(START_MS + 700 + BT_MODULE_SILENCE_MS, &frame));
    CHECK_EQUAL(1, module.status.remote.pauses);
    CHECK_EQUAL(1, module.status.remote.plays);
    CHECK_EQUAL(1, module.status.remote.previouses);
    CHECK_EQUAL(2, module.status.remote.nexts);
    CHECK(module.status.streaming);
}

static void a_play_report_without_a_stream_is_not_answered(void)
{
    BtModuleFrame frame;
    connected_for(START_MS + 400);
    receive_text("CC", START_MS + 600);
    CHECK(!poll(START_MS + 600 + BT_MODULE_SILENCE_MS, &frame));
    receive_text("CP", START_MS + 700);
    CHECK(!poll(START_MS + 700 + BT_MODULE_SILENCE_MS, &frame));
}

static void a_new_link_is_told_the_stream_is_off(void)
{
    static const uint8_t STREAM_OFF[] = {'C', 'Y', '0', 0x00, 0x00};
    BtModuleFrame frame;
    power_on_ready();
    receive_connected(PHONES, "WH-1000XM4", START_MS + 400);
    CHECK(poll(START_MS + 400 + BT_MODULE_SILENCE_MS, &frame));
    CHECK(frame_bytes_are(&frame, STREAM_OFF, sizeof STREAM_OFF));
    CHECK(!poll(START_MS + 410, &frame));
}

static void streaming_needs_a_connection(void)
{
    BtModuleFrame frame;
    power_on_ready();
    CHECK(!bt_module_set_stream(&module, true, START_MS + 500, &frame));
    CHECK(!module.status.streaming);
}

static void a_dropped_link_ends_the_stream(void)
{
    BtModuleFrame frame;
    connected_for(START_MS + 400);
    bt_module_set_stream(&module, true, START_MS + 500, &frame);
    poll(START_MS + 700, &frame);
    receive_text("CQI", START_MS + 800);
    settle(START_MS + 800);
    CHECK(!module.status.streaming);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_powered_module_waits_for_its_ready_report),
        TEST_CASE(a_frame_split_across_reads_waits_for_the_rest),
        TEST_CASE(a_cut_off_frame_is_dropped_after_a_pause),
        TEST_CASE(a_long_burst_is_read_in_full),
        TEST_CASE(no_ready_report_within_five_seconds_means_no_reply),
        TEST_CASE(noise_before_the_ready_report_is_skipped),
        TEST_CASE(a_scan_sends_cs00),
        TEST_CASE(a_scan_needs_a_ready_module),
        TEST_CASE(scan_results_fill_the_device_list),
        TEST_CASE(csen_ends_the_scan),
        TEST_CASE(repeated_and_unnamed_devices_are_dropped),
        TEST_CASE(the_list_holds_six_devices),
        TEST_CASE(names_up_to_31_bytes_fit),
        TEST_CASE(an_oversized_scan_frame_ends_the_scan),
        TEST_CASE(an_unanswered_scan_is_sent_once_more_after_four_seconds),
        TEST_CASE(an_answered_scan_is_not_repeated),
        TEST_CASE(a_scan_stops_with_cp00_after_thirty_seconds),
        TEST_CASE(a_scan_pauses_the_status_poll),
        TEST_CASE(a_ready_module_is_asked_for_its_status_every_three_seconds),
        TEST_CASE(status_replies_set_the_link_letter),
        TEST_CASE(a_connected_reply_skips_its_payload),
        TEST_CASE(the_last_received_bytes_are_kept),
        TEST_CASE(frames_are_counted),
        TEST_CASE(powering_off_forgets_the_session),
        TEST_CASE(connecting_sends_cl06_with_the_picked_address),
        TEST_CASE(connecting_needs_a_ready_module),
        TEST_CASE(a_remembered_device_connects_without_a_scan),
        TEST_CASE(connecting_works_while_still_scanning),
        TEST_CASE(clok_reports_paired),
        TEST_CASE(cler_and_cher_report_a_failed_pairing),
        TEST_CASE(an_unanswered_connect_retries_once_with_ch06),
        TEST_CASE(a_connect_without_a_result_is_cancelled_after_thirty_seconds),
        TEST_CASE(cqo_reports_the_connected_device),
        TEST_CASE(a_long_connected_name_is_cut),
        TEST_CASE(chok_connects_the_picked_device),
        TEST_CASE(a_connected_module_is_still_asked_for_its_status),
        TEST_CASE(another_status_letter_means_the_link_dropped),
        TEST_CASE(disconnecting_sends_ct00_and_ctok_confirms_it),
        TEST_CASE(disconnecting_needs_a_link),
        TEST_CASE(a_disconnect_cancels_a_pending_connect),
        TEST_CASE(module_messages_are_kept_without_status_or_scan_replies),
        TEST_CASE(a_new_link_is_told_the_stream_is_off),
        TEST_CASE(a_play_report_pauses_status_queries_for_fifty_seconds),
        TEST_CASE(a_play_report_after_clok_means_connected),
        TEST_CASE(a_play_report_before_a_link_keeps_the_status_poll),
        TEST_CASE(ctok_after_clok_clears_the_pairing),
        TEST_CASE(a_dropped_link_clears_the_pairing),
        TEST_CASE(buds_controls_are_counted_without_sending_anything),
        TEST_CASE(a_play_report_without_a_stream_is_not_answered),
        TEST_CASE(the_first_stream_start_sends_only_cy0_1e),
        TEST_CASE(resuming_after_a_stop_sends_the_play_notice),
        TEST_CASE(stopping_the_stream_sends_cy0_00_then_the_pause_notice),
        TEST_CASE(streaming_needs_a_connection),
        TEST_CASE(a_dropped_link_ends_the_stream),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

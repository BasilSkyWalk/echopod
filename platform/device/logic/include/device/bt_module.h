#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hal/bluetooth.h"

enum {
    BT_MODULE_CHUNK_CAPACITY = 128,
    BT_MODULE_FRAME_CAPACITY = 10,
    BT_MODULE_SILENCE_MS = 2,
    BT_MODULE_READY_TIMEOUT_MS = 5000,
    BT_MODULE_SCAN_RETRY_MS = 4000,
    BT_MODULE_SCAN_LIMIT_MS = 30000,
    BT_MODULE_QUERY_PERIOD_MS = 3000,
    BT_MODULE_LINK_QUERY_MS = 500,
    BT_MODULE_CONNECT_QUERY_MS = 1000,
    BT_MODULE_GAP_LIMIT_S = 99,
    BT_MODULE_CONNECT_RETRY_MS = 4000,
    BT_MODULE_CONNECT_LIMIT_MS = 30000,
    BT_MODULE_PLAY_NOTICE_DELAY_MS = 100,
    BT_MODULE_QUERY_PAUSE_MS = 50000
};

typedef struct BtModuleFrame {
    uint8_t bytes[BT_MODULE_FRAME_CAPACITY];
    uint8_t length;
} BtModuleFrame;

typedef struct BtModule {
    HalBluetoothStatus status;
    uint8_t chunk[BT_MODULE_CHUNK_CAPACITY];
    uint8_t chunk_length;
    uint32_t last_byte_ms;
    uint32_t state_since_ms;
    uint32_t last_query_ms;
    HalBluetoothDevice target;
    bool answered;
    bool repeated;
    bool stream_pending;
    uint8_t play_state;
    bool query_paused;
    bool resend_stream;
    bool resend_on;
    uint32_t query_paused_ms;
    uint32_t stream_changed_ms;
    uint32_t last_event_ms;
} BtModule;

void bt_module_power_on(BtModule *module, uint32_t now_ms);
void bt_module_power_off(BtModule *module);
void bt_module_receive(BtModule *module, uint8_t byte, uint32_t now_ms);
bool bt_module_scan(BtModule *module, uint32_t now_ms, BtModuleFrame *frame);
bool bt_module_stop_scan(BtModule *module, uint32_t now_ms, BtModuleFrame *frame);
bool bt_module_connect(BtModule *module, const HalBluetoothDevice *device, uint32_t now_ms,
                       BtModuleFrame *frame);
bool bt_module_disconnect(BtModule *module, uint32_t now_ms, BtModuleFrame *frame);
bool bt_module_set_stream(BtModule *module, bool on, uint32_t now_ms, BtModuleFrame *frame);
bool bt_module_poll(BtModule *module, uint32_t now_ms, BtModuleFrame *frame);

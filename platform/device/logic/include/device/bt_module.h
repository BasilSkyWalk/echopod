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
    BT_MODULE_QUERY_PERIOD_MS = 3000
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
    bool scan_answered;
    bool scan_repeated;
} BtModule;

void bt_module_power_on(BtModule *module, uint32_t now_ms);
void bt_module_power_off(BtModule *module);
void bt_module_receive(BtModule *module, uint8_t byte, uint32_t now_ms);
bool bt_module_scan(BtModule *module, uint32_t now_ms, BtModuleFrame *frame);
bool bt_module_poll(BtModule *module, uint32_t now_ms, BtModuleFrame *frame);

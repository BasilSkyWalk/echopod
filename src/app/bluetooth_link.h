#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bluetooth_store.h"
#include "hal/bluetooth.h"
#include "player.h"
#include "ui/toast.h"

enum {
    BLUETOOTH_LINK_RETRY_MS = 5000,
    BLUETOOTH_LINK_CONFIRM_MS = 10000,
    BLUETOOTH_LINK_RESET_MS = 2000,
    BLUETOOTH_LINK_ECHO_MS = 3000,
    BLUETOOTH_LINK_IDLE_OFF_MS = 300000,
    BLUETOOTH_LINK_TOAST_CAPACITY = 48
};

typedef enum BluetoothDeviceState {
    BLUETOOTH_DEVICE_IDLE,
    BLUETOOTH_DEVICE_CONNECTING,
    BLUETOOTH_DEVICE_CONNECTED,
    BLUETOOTH_DEVICE_PLAYING
} BluetoothDeviceState;

typedef struct BluetoothLink {
    Toast *toast;
    BluetoothSaved saved;
    bool browsing;
    bool forgetting;
    bool connected;
    bool streamed;
    bool dozing;
    bool wired;
    bool searched;
    bool search_now;
    bool held;
    bool attempting;
    bool has_target;
    bool paired_seen;
    bool scan_tried;
    bool buds_paused;
    HalBluetoothDevice target;
    HalBluetoothDevice linked;
    uint32_t retried_ms;
    uint32_t paired_ms;
    uint32_t unlinked_ms;
    uint32_t changed_ms;
    HalBluetoothRemote seen;
    char toast_text[BLUETOOTH_LINK_TOAST_CAPACITY];
} BluetoothLink;

void bluetooth_link_start(BluetoothLink *link, Toast *toast, uint32_t now_ms);
void bluetooth_link_set_enabled(BluetoothLink *link, bool enabled, uint32_t now_ms);
void bluetooth_link_browse(BluetoothLink *link, bool browsing, uint32_t now_ms);
void bluetooth_link_choose(BluetoothLink *link, int index, uint32_t now_ms);
void bluetooth_link_pair(BluetoothLink *link, const HalBluetoothDevice *device, uint32_t now_ms);
void bluetooth_link_search(BluetoothLink *link, uint32_t now_ms);
void bluetooth_link_forget(BluetoothLink *link, int index);
void bluetooth_link_sleep(BluetoothLink *link);
void bluetooth_link_step(BluetoothLink *link, Player *player, uint32_t now_ms);
BluetoothDeviceState bluetooth_link_device_state(const BluetoothLink *link, int index);
bool bluetooth_link_knows(const BluetoothLink *link, const HalBluetoothDevice *device);
bool bluetooth_link_nearby(const BluetoothLink *link, int index);
const char *bluetooth_link_summary(const BluetoothLink *link);

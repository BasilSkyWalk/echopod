#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bluetooth_store.h"
#include "hal/bluetooth.h"
#include "player.h"
#include "ui/toast.h"

enum {
    BLUETOOTH_LINK_RETRY_MS = 15000,
    BLUETOOTH_LINK_CONFIRM_MS = 120000,
    BLUETOOTH_LINK_IDLE_OFF_MS = 300000,
    BLUETOOTH_LINK_TOAST_CAPACITY = 48
};

typedef struct BluetoothLink {
    Toast *toast;
    BluetoothSaved saved;
    bool browsing;
    bool reconnecting;
    bool forgetting;
    bool connected;
    bool streamed;
    bool dozing;
    bool wired;
    bool searched;
    uint32_t retried_ms;
    uint32_t unlinked_ms;
    HalBluetoothRemote seen;
    char toast_text[BLUETOOTH_LINK_TOAST_CAPACITY];
} BluetoothLink;

void bluetooth_link_start(BluetoothLink *link, Toast *toast, uint32_t now_ms);
void bluetooth_link_set_enabled(BluetoothLink *link, bool enabled, uint32_t now_ms);
void bluetooth_link_browse(BluetoothLink *link, bool browsing, uint32_t now_ms);
void bluetooth_link_pair(BluetoothLink *link, const HalBluetoothDevice *device, uint32_t now_ms);
void bluetooth_link_connect_saved(BluetoothLink *link, uint32_t now_ms);
void bluetooth_link_forget(BluetoothLink *link);
void bluetooth_link_sleep(BluetoothLink *link);
void bluetooth_link_step(BluetoothLink *link, Player *player, uint32_t now_ms);
const char *bluetooth_link_summary(const BluetoothLink *link);

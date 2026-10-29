#pragma once

#include <stdbool.h>

#include "hal/bluetooth.h"

extern const char BLUETOOTH_STORE_PATH[];

typedef struct BluetoothSaved {
    bool enabled;
    bool has_device;
    HalBluetoothDevice device;
} BluetoothSaved;

void bluetooth_store_load(BluetoothSaved *saved);
void bluetooth_store_save(const BluetoothSaved *saved);

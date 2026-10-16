#include <string.h>

#include "hal/bluetooth.h"
#include "hal/clock.h"

enum {
    READY_AFTER_MS = 300,
    DEVICE_EVERY_MS = 1000,
    SCAN_LENGTH_MS = 3000,
    CONNECT_LENGTH_MS = 1500
};

typedef struct SimDevice {
    uint8_t address[HAL_BLUETOOTH_ADDRESS_SIZE];
    const char *name;
} SimDevice;

static const SimDevice DEVICES[] = {
    {{0x11, 0x22, 0x33, 0x44, 0x55, 0x66}, "Sim Headphones"},
    {{0xA0, 0xB1, 0xC2, 0xD3, 0xE4, 0xF5}, "Sim Speaker"},
};

typedef struct SimBluetooth {
    HalBluetoothStatus status;
    uint32_t since_ms;
    uint8_t target;
} SimBluetooth;

static SimBluetooth bluetooth;

static void enter_state(HalBluetoothState state)
{
    bluetooth.status.state = state;
    bluetooth.since_ms = hal_clock_milliseconds();
}

void hal_bluetooth_set_power(bool on)
{
    bluetooth = (SimBluetooth){0};
    enter_state(on ? HAL_BLUETOOTH_STARTING : HAL_BLUETOOTH_OFF);
}

void hal_bluetooth_scan(void)
{
    if (bluetooth.status.state != HAL_BLUETOOTH_READY) {
        return;
    }
    bluetooth.status.device_count = 0;
    enter_state(HAL_BLUETOOTH_SCANNING);
}

void hal_bluetooth_connect(uint8_t index)
{
    HalBluetoothState state = bluetooth.status.state;
    bool idle = state == HAL_BLUETOOTH_READY || state == HAL_BLUETOOTH_SCANNING;
    if (!idle || index >= bluetooth.status.device_count) {
        return;
    }
    bluetooth.target = index;
    bluetooth.status.pairing = HAL_BLUETOOTH_PAIRING_NONE;
    enter_state(HAL_BLUETOOTH_CONNECTING);
}

void hal_bluetooth_disconnect(void)
{
    HalBluetoothState state = bluetooth.status.state;
    if (state == HAL_BLUETOOTH_CONNECTED || state == HAL_BLUETOOTH_CONNECTING) {
        enter_state(HAL_BLUETOOTH_READY);
    }
}

static void finish_connect(void)
{
    bluetooth.status.pairing = HAL_BLUETOOTH_PAIRED;
    bluetooth.status.connected = bluetooth.status.devices[bluetooth.target];
    enter_state(HAL_BLUETOOTH_CONNECTED);
}

static void add_found_devices(uint32_t elapsed_ms)
{
    uint32_t found = elapsed_ms / DEVICE_EVERY_MS;
    uint32_t available = sizeof DEVICES / sizeof DEVICES[0];
    while (bluetooth.status.device_count < found && bluetooth.status.device_count < available) {
        const SimDevice *device = &DEVICES[bluetooth.status.device_count];
        HalBluetoothDevice *entry = &bluetooth.status.devices[bluetooth.status.device_count++];
        memcpy(entry->address, device->address, sizeof entry->address);
        strncpy(entry->name, device->name, sizeof entry->name - 1);
    }
}

void hal_bluetooth_service(void)
{
    uint32_t elapsed_ms = hal_clock_milliseconds() - bluetooth.since_ms;
    if (bluetooth.status.state == HAL_BLUETOOTH_STARTING && elapsed_ms >= READY_AFTER_MS) {
        bluetooth.status.version = '5';
        enter_state(HAL_BLUETOOTH_READY);
    } else if (bluetooth.status.state == HAL_BLUETOOTH_CONNECTING &&
               elapsed_ms >= CONNECT_LENGTH_MS) {
        finish_connect();
    } else if (bluetooth.status.state == HAL_BLUETOOTH_SCANNING) {
        add_found_devices(elapsed_ms);
        if (elapsed_ms >= SCAN_LENGTH_MS) {
            enter_state(HAL_BLUETOOTH_READY);
        }
    }
}

const HalBluetoothStatus *hal_bluetooth_status(void)
{
    return &bluetooth.status;
}

void hal_bluetooth_set_stream(bool on)
{
    if (bluetooth.status.state == HAL_BLUETOOTH_CONNECTED) {
        bluetooth.status.streaming = on;
    }
}

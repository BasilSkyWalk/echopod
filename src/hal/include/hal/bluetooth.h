#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    HAL_BLUETOOTH_ADDRESS_SIZE = 6,
    HAL_BLUETOOTH_NAME_CAPACITY = 32,
    HAL_BLUETOOTH_DEVICE_CAPACITY = 6,
    HAL_BLUETOOTH_RECENT_CAPACITY = 12,
    HAL_BLUETOOTH_EVENT_CAPACITY = 12
};

typedef enum HalBluetoothState {
    HAL_BLUETOOTH_OFF,
    HAL_BLUETOOTH_STARTING,
    HAL_BLUETOOTH_NO_REPLY,
    HAL_BLUETOOTH_READY,
    HAL_BLUETOOTH_SCANNING,
    HAL_BLUETOOTH_CONNECTING,
    HAL_BLUETOOTH_CONNECTED
} HalBluetoothState;

typedef enum HalBluetoothPairing {
    HAL_BLUETOOTH_PAIRING_NONE,
    HAL_BLUETOOTH_PAIRED,
    HAL_BLUETOOTH_PAIRING_FAILED
} HalBluetoothPairing;

typedef struct HalBluetoothDevice {
    uint8_t address[HAL_BLUETOOTH_ADDRESS_SIZE];
    char name[HAL_BLUETOOTH_NAME_CAPACITY];
} HalBluetoothDevice;

typedef struct HalBluetoothLink {
    uint32_t received;
    uint32_t overruns;
    uint32_t frames;
    uint16_t fifo_depth;
    uint16_t fifo_peak;
    uint8_t recent_length;
    uint8_t recent[HAL_BLUETOOTH_RECENT_CAPACITY];
    uint8_t event_count;
    char events[HAL_BLUETOOTH_EVENT_CAPACITY][2];
    uint8_t gaps[HAL_BLUETOOTH_EVENT_CAPACITY];
} HalBluetoothLink;

typedef struct HalBluetoothRemote {
    uint8_t plays;
    uint8_t pauses;
    uint8_t nexts;
    uint8_t previouses;
} HalBluetoothRemote;

typedef struct HalBluetoothStatus {
    HalBluetoothState state;
    char version;
    char link_status;
    HalBluetoothPairing pairing;
    bool streaming;
    HalBluetoothDevice connected;
    uint8_t device_count;
    uint8_t found_now;
    HalBluetoothDevice devices[HAL_BLUETOOTH_DEVICE_CAPACITY];
    HalBluetoothLink link;
    HalBluetoothRemote remote;
} HalBluetoothStatus;

void hal_bluetooth_set_power(bool on);
void hal_bluetooth_scan(void);
void hal_bluetooth_stop_scan(void);
void hal_bluetooth_connect(const HalBluetoothDevice *device);
void hal_bluetooth_disconnect(void);
void hal_bluetooth_set_stream(bool on);
void hal_bluetooth_service(void);
const HalBluetoothStatus *hal_bluetooth_status(void);

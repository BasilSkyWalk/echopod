#include "bluetooth_store.h"

#include <stddef.h>
#include <stdint.h>

#include "hal/storage.h"

const char BLUETOOTH_STORE_PATH[] = "/.echo/bluetooth.bin";

enum {
    STORE_VERSION = 1,
    HEADER_SIZE = 3,
    ADDRESS_AT = HEADER_SIZE,
    NAME_AT = ADDRESS_AT + HAL_BLUETOOTH_ADDRESS_SIZE,
    ENABLED_AT = NAME_AT + HAL_BLUETOOTH_NAME_CAPACITY,
    RECORD_SIZE = ENABLED_AT + 1
};

static const uint8_t HEADER[HEADER_SIZE] = {'B', 'T', STORE_VERSION};

static bool has_address(const HalBluetoothDevice *device)
{
    for (size_t index = 0; index < HAL_BLUETOOTH_ADDRESS_SIZE; index++) {
        if (device->address[index] != 0) {
            return true;
        }
    }
    return false;
}

static bool read_record(uint8_t *record)
{
    HalFile file;
    if (!hal_file_open(&file, BLUETOOTH_STORE_PATH)) {
        return false;
    }
    bool read = file.size >= RECORD_SIZE && hal_file_read(file, 0, record, RECORD_SIZE);
    hal_file_close(&file);
    for (size_t index = 0; read && index < HEADER_SIZE; index++) {
        read = record[index] == HEADER[index];
    }
    return read;
}

void bluetooth_store_load(BluetoothSaved *saved)
{
    uint8_t record[RECORD_SIZE];
    *saved = (BluetoothSaved){.enabled = false, .has_device = false};
    if (!read_record(record)) {
        return;
    }
    for (size_t index = 0; index < HAL_BLUETOOTH_ADDRESS_SIZE; index++) {
        saved->device.address[index] = record[ADDRESS_AT + index];
    }
    for (size_t index = 0; index < HAL_BLUETOOTH_NAME_CAPACITY; index++) {
        saved->device.name[index] = (char)record[NAME_AT + index];
    }
    saved->device.name[HAL_BLUETOOTH_NAME_CAPACITY - 1] = '\0';
    saved->has_device = has_address(&saved->device);
    saved->enabled = record[ENABLED_AT] == 1;
}

void bluetooth_store_save(const BluetoothSaved *saved)
{
    uint8_t record[RECORD_SIZE] = {0};
    for (size_t index = 0; index < HEADER_SIZE; index++) {
        record[index] = HEADER[index];
    }
    for (size_t index = 0; saved->has_device && index < HAL_BLUETOOTH_ADDRESS_SIZE; index++) {
        record[ADDRESS_AT + index] = saved->device.address[index];
    }
    for (size_t index = 0; saved->has_device && index < HAL_BLUETOOTH_NAME_CAPACITY; index++) {
        record[NAME_AT + index] = (uint8_t)saved->device.name[index];
    }
    record[ENABLED_AT] = saved->enabled ? 1 : 0;
    (void)hal_file_write(BLUETOOTH_STORE_PATH, record, RECORD_SIZE);
}

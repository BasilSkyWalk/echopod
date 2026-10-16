#pragma once

#include <stdbool.h>

#include "hal/bluetooth.h"
#include "ui/screen.h"
#include "ui/theme.h"

enum {
    BLUETOOTH_SCREEN_STATUS_LINES = 3,
    BLUETOOTH_SCREEN_LINE_COUNT = BLUETOOTH_SCREEN_STATUS_LINES + HAL_BLUETOOTH_DEVICE_CAPACITY,
    BLUETOOTH_SCREEN_LINE_CAPACITY = 48
};

typedef struct BluetoothScreen {
    Screen screen;
    const Theme *theme;
    bool scanned;
    char lines[BLUETOOTH_SCREEN_LINE_COUNT][BLUETOOTH_SCREEN_LINE_CAPACITY];
} BluetoothScreen;

Screen *bluetooth_screen_init(BluetoothScreen *bluetooth, const Theme *theme);
const char *bluetooth_screen_line(const BluetoothScreen *bluetooth, int index);

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bluetooth_link.h"
#include "hal/bluetooth.h"
#include "ui/list_view.h"
#include "ui/screen.h"
#include "ui/theme.h"

enum {
    BLUETOOTH_SCREEN_HINT_LINE = 0,
    BLUETOOTH_SCREEN_DETAIL_LINES = 3,
    BLUETOOTH_SCREEN_LINE_COUNT = BLUETOOTH_SCREEN_DETAIL_LINES + 1,
    BLUETOOTH_SCREEN_LINE_CAPACITY = 64
};

typedef struct BluetoothScreen {
    Screen screen;
    const Theme *theme;
    BluetoothLink *link;
    bool details;
    bool pairing;
    HalBluetoothDevice paired;
    ListModel model;
    ListView list;
    uint32_t now_ms;
    uint8_t nearby_count;
    uint8_t nearby[HAL_BLUETOOTH_DEVICE_CAPACITY];
    char lines[BLUETOOTH_SCREEN_LINE_COUNT][BLUETOOTH_SCREEN_LINE_CAPACITY];
} BluetoothScreen;

Screen *bluetooth_screen_init(BluetoothScreen *bluetooth, BluetoothLink *link, const Theme *theme);
ScreenAction bluetooth_screen_open(void *bluetooth);
ScreenAction bluetooth_screen_open_details(void *bluetooth);
const char *bluetooth_screen_summary(const void *bluetooth);
int bluetooth_screen_row_count(const BluetoothScreen *bluetooth);
const char *bluetooth_screen_row(const BluetoothScreen *bluetooth, int index);
const char *bluetooth_screen_row_value(const BluetoothScreen *bluetooth, int index);
const char *bluetooth_screen_line(const BluetoothScreen *bluetooth, int index);

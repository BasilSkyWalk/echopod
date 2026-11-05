#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    VBUS_DEBOUNCE_MS = 100,
    VBUS_DEBOUNCE_READINGS = 3
};

typedef struct VbusDebounce {
    bool present;
    uint32_t absent_since_ms;
    uint32_t absent_readings;
    uint32_t dips;
} VbusDebounce;

void vbus_debounce_start(VbusDebounce *vbus);
bool vbus_debounce_present(VbusDebounce *vbus, bool reading, uint32_t now_ms);

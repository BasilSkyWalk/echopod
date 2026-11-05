#include "device/vbus_debounce.h"

void vbus_debounce_start(VbusDebounce *vbus)
{
    *vbus = (VbusDebounce){.present = true};
}

bool vbus_debounce_present(VbusDebounce *vbus, bool reading, uint32_t now_ms)
{
    if (!vbus->present) {
        return false;
    }
    if (reading) {
        if (vbus->absent_readings > 0) {
            vbus->dips++;
        }
        vbus->absent_readings = 0;
        return true;
    }
    if (vbus->absent_readings == 0) {
        vbus->absent_since_ms = now_ms;
    }
    vbus->absent_readings++;
    if (vbus->absent_readings >= VBUS_DEBOUNCE_READINGS &&
        now_ms - vbus->absent_since_ms >= VBUS_DEBOUNCE_MS) {
        vbus->present = false;
    }
    return vbus->present;
}

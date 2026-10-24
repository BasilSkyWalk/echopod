#include "device/usb_fallback.h"

void usb_fallback_start(UsbFallback *fallback, bool high_speed, uint32_t now_ms)
{
    *fallback = (UsbFallback){.watching = high_speed, .started_ms = now_ms};
}

bool usb_fallback_due(UsbFallback *fallback, bool working, uint32_t now_ms)
{
    if (!fallback->watching) {
        return false;
    }
    if (working) {
        fallback->watching = false;
        return false;
    }
    if (now_ms - fallback->started_ms < USB_FALLBACK_MS) {
        return false;
    }
    fallback->watching = false;
    return true;
}

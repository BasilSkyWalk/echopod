#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    USB_FALLBACK_MS = 10000
};

typedef struct UsbFallback {
    bool watching;
    uint32_t started_ms;
} UsbFallback;

void usb_fallback_start(UsbFallback *fallback, bool high_speed, uint32_t now_ms);
bool usb_fallback_due(UsbFallback *fallback, bool working, uint32_t now_ms);

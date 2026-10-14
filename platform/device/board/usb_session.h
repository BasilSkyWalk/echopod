#pragma once

#include <stdbool.h>

#include "usb.h"

typedef enum UsbSessionEnd {
    USB_SESSION_NO_HOST,
    USB_SESSION_EJECTED,
    USB_SESSION_UNPLUGGED,
    USB_SESSION_FAILED
} UsbSessionEnd;

typedef struct UsbReport {
    bool internal_ready;
    UsbConnectStep connect_step;
    UsbProbe after_power_up;
    UsbProbe after_wait;
} UsbReport;

UsbSessionEnd usb_session_run(void);
const UsbReport *usb_session_report(void);

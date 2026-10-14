#pragma once

typedef enum UsbSessionEnd {
    USB_SESSION_NO_HOST,
    USB_SESSION_EJECTED,
    USB_SESSION_UNPLUGGED,
    USB_SESSION_FAILED
} UsbSessionEnd;

UsbSessionEnd usb_session_run(void);

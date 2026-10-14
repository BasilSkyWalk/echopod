#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/msc.h"
#include "device/usb_device.h"

typedef struct UsbPort {
    bool dedicated_fifos;
    bool configured;
    bool in_busy;
    bool setup_pending;
    uint32_t setup[2];
    uint32_t out_packet[USB_PACKET_SIZE / 4];
    UsbDevice device;
    Msc *msc;
} UsbPort;

bool usb_vbus_present(void);
void usb_power_up(void);
void usb_power_down(void);
bool usb_connect(UsbPort *port);
bool usb_wait_for_host(UsbPort *port, uint32_t timeout_ms);
void usb_poll(UsbPort *port);
bool usb_reply_pending(const UsbPort *port);

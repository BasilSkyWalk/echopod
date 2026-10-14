#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    USB_ENDPOINT_BULK_IN = 0x81,
    USB_ENDPOINT_BULK_OUT = 0x01,
    USB_PACKET_SIZE = 64,
    USB_SERIAL_CAPACITY = 16
};

typedef struct UsbSetup {
    uint8_t request_type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
} UsbSetup;

typedef enum UsbReplyKind {
    USB_REPLY_STALL,
    USB_REPLY_DATA,
    USB_REPLY_STATUS
} UsbReplyKind;

typedef enum UsbAction {
    USB_ACTION_NONE,
    USB_ACTION_SET_ADDRESS,
    USB_ACTION_CONFIGURE,
    USB_ACTION_CLEAR_HALT,
    USB_ACTION_MSC_RESET
} UsbAction;

typedef struct UsbReply {
    UsbReplyKind kind;
    const uint8_t *data;
    uint16_t length;
    UsbAction action;
    uint16_t value;
} UsbReply;

typedef struct UsbDevice {
    uint8_t configuration;
    uint8_t lun_count;
    uint8_t scratch[4];
    uint8_t serial[2 + 2 * USB_SERIAL_CAPACITY];
} UsbDevice;

UsbSetup usb_setup_parse(const uint8_t packet[8]);
void usb_device_init(UsbDevice *device, const char *serial, uint8_t lun_count);
UsbReply usb_device_setup(UsbDevice *device, UsbSetup setup);

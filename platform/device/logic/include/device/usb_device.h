#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    USB_ENDPOINT_BULK_IN = 0x81,
    USB_ENDPOINT_BULK_OUT = 0x01,
    USB_PACKET_SIZE = 64,
    USB_HIGH_SPEED_PACKET_SIZE = 512,
    USB_CONFIGURATION_LENGTH = 32,
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
    USB_REPLY_STATUS,
    USB_REPLY_RECEIVE
} UsbReplyKind;

typedef enum UsbAction {
    USB_ACTION_NONE,
    USB_ACTION_SET_ADDRESS,
    USB_ACTION_CONFIGURE,
    USB_ACTION_CLEAR_HALT,
    USB_ACTION_MSC_RESET,
    USB_ACTION_STREAM
} UsbAction;

typedef struct UsbReply {
    UsbReplyKind kind;
    const uint8_t *data;
    uint16_t length;
    UsbAction action;
    uint16_t value;
} UsbReply;

typedef enum UsbSpeed {
    USB_SPEED_FULL,
    USB_SPEED_HIGH
} UsbSpeed;

typedef struct UsbDevice {
    uint8_t configuration;
    uint8_t lun_count;
    uint8_t scratch[4];
    uint8_t serial[2 + 2 * USB_SERIAL_CAPACITY];
    bool high_speed_allowed;
    UsbSpeed speed;
    uint8_t configuration_reply[USB_CONFIGURATION_LENGTH];
} UsbDevice;

UsbSetup usb_setup_parse(const uint8_t packet[8]);
void usb_device_init(UsbDevice *device, const char *serial, uint8_t lun_count);
UsbReply usb_device_setup(UsbDevice *device, UsbSetup setup);
void usb_device_allow_high_speed(UsbDevice *device, bool allowed);
void usb_device_set_speed(UsbDevice *device, UsbSpeed speed);
uint16_t usb_device_bulk_packet_size(const UsbDevice *device);

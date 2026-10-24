#include "device/usb_device.h"

#include <stddef.h>

enum {
    TYPE_STANDARD_DEVICE_IN = 0x80,
    TYPE_STANDARD_INTERFACE_IN = 0x81,
    TYPE_STANDARD_ENDPOINT_IN = 0x82,
    TYPE_STANDARD_DEVICE_OUT = 0x00,
    TYPE_STANDARD_INTERFACE_OUT = 0x01,
    TYPE_STANDARD_ENDPOINT_OUT = 0x02,
    TYPE_CLASS_INTERFACE_IN = 0xA1,
    TYPE_CLASS_INTERFACE_OUT = 0x21,
    REQUEST_GET_STATUS = 0,
    REQUEST_CLEAR_FEATURE = 1,
    REQUEST_SET_ADDRESS = 5,
    REQUEST_GET_DESCRIPTOR = 6,
    REQUEST_GET_CONFIGURATION = 8,
    REQUEST_SET_CONFIGURATION = 9,
    REQUEST_GET_INTERFACE = 10,
    REQUEST_SET_INTERFACE = 11,
    REQUEST_MSC_GET_MAX_LUN = 0xFE,
    REQUEST_MSC_RESET = 0xFF,
    DESCRIPTOR_DEVICE = 1,
    DESCRIPTOR_CONFIGURATION = 2,
    DESCRIPTOR_STRING = 3,
    DESCRIPTOR_DEVICE_QUALIFIER = 6,
    DESCRIPTOR_OTHER_SPEED_CONFIGURATION = 7,
    CONFIGURATION_TYPE_OFFSET = 1,
    BULK_IN_PACKET_OFFSET = 22,
    BULK_OUT_PACKET_OFFSET = 29,
    FEATURE_ENDPOINT_HALT = 0,
    STRING_SERIAL = 3
};

static const uint8_t DEVICE_DESCRIPTOR[] = {
    18,   1,    0x00, 0x02, 0x00, 0x00, 0x00, USB_PACKET_SIZE, 0x09, 0x12, 0x01,
    0x00, 0x00, 0x01, 1,    2,    3,    1,
};

static const uint8_t CONFIGURATION_DESCRIPTOR[] = {
    9,
    2,
    32,
    0,
    1,
    1,
    0,
    0x80,
    50,
    9,
    4,
    0,
    0,
    2,
    0x08,
    0x06,
    0x50,
    0,
    7,
    5,
    USB_ENDPOINT_BULK_IN,
    0x02,
    USB_PACKET_SIZE,
    0,
    0,
    7,
    5,
    USB_ENDPOINT_BULK_OUT,
    0x02,
    USB_PACKET_SIZE,
    0,
    0,
};

static const uint8_t DEVICE_QUALIFIER[] = {10, 6, 0x00, 0x02, 0x00, 0x00, 0x00, USB_PACKET_SIZE,
                                           1,  0};

static const uint8_t LANGUAGES[] = {4, 3, 0x09, 0x04};
static const uint8_t PRODUCT[] = {16, 3, 'E', 0, 'c', 0, 'h', 0, 'o', 0, 'P', 0, 'o', 0, 'd', 0};

UsbSetup usb_setup_parse(const uint8_t packet[8])
{
    return (UsbSetup){packet[0], packet[1], (uint16_t)(packet[2] | packet[3] << 8),
                      (uint16_t)(packet[4] | packet[5] << 8),
                      (uint16_t)(packet[6] | packet[7] << 8)};
}

void usb_device_init(UsbDevice *device, const char *serial, uint8_t lun_count)
{
    *device = (UsbDevice){.configuration = 0, .lun_count = lun_count};
    size_t length = 0;
    while (serial[length] != '\0' && length < USB_SERIAL_CAPACITY) {
        device->serial[2 + 2 * length] = (uint8_t)serial[length];
        length++;
    }
    device->serial[0] = (uint8_t)(2 + 2 * length);
    device->serial[1] = DESCRIPTOR_STRING;
}

static UsbReply stall(void)
{
    return (UsbReply){USB_REPLY_STALL, NULL, 0, USB_ACTION_NONE, 0};
}

static UsbReply status(UsbAction action, uint16_t value)
{
    return (UsbReply){USB_REPLY_STATUS, NULL, 0, action, value};
}

static UsbReply data(const uint8_t *bytes, size_t length, UsbSetup setup)
{
    uint16_t sent = (uint16_t)(length < setup.length ? length : setup.length);
    return (UsbReply){USB_REPLY_DATA, bytes, sent, USB_ACTION_NONE, 0};
}

static UsbReply string_descriptor(const UsbDevice *device, uint8_t index, UsbSetup setup)
{
    static const uint8_t *const STRINGS[] = {LANGUAGES, PRODUCT, PRODUCT};
    if (index == STRING_SERIAL) {
        return data(device->serial, device->serial[0], setup);
    }
    if (index >= sizeof STRINGS / sizeof STRINGS[0]) {
        return stall();
    }
    return data(STRINGS[index], STRINGS[index][0], setup);
}

static uint16_t packet_size(UsbSpeed speed)
{
    return speed == USB_SPEED_HIGH ? USB_HIGH_SPEED_PACKET_SIZE : USB_PACKET_SIZE;
}

static void put_packet_size(uint8_t *bytes, uint16_t size)
{
    bytes[0] = (uint8_t)size;
    bytes[1] = (uint8_t)(size >> 8);
}

static UsbReply configuration(UsbDevice *device, uint8_t type, UsbSpeed speed, UsbSetup setup)
{
    uint8_t *reply = device->configuration_reply;
    for (size_t index = 0; index < sizeof CONFIGURATION_DESCRIPTOR; index++) {
        reply[index] = CONFIGURATION_DESCRIPTOR[index];
    }
    reply[CONFIGURATION_TYPE_OFFSET] = type;
    put_packet_size(reply + BULK_IN_PACKET_OFFSET, packet_size(speed));
    put_packet_size(reply + BULK_OUT_PACKET_OFFSET, packet_size(speed));
    return data(reply, sizeof CONFIGURATION_DESCRIPTOR, setup);
}

static UsbSpeed other_speed(UsbSpeed speed)
{
    return speed == USB_SPEED_HIGH ? USB_SPEED_FULL : USB_SPEED_HIGH;
}

static UsbReply descriptor(UsbDevice *device, UsbSetup setup)
{
    uint8_t type = (uint8_t)(setup.value >> 8);
    uint8_t index = (uint8_t)setup.value;
    switch (type) {
    case DESCRIPTOR_DEVICE:
        return data(DEVICE_DESCRIPTOR, sizeof DEVICE_DESCRIPTOR, setup);
    case DESCRIPTOR_CONFIGURATION:
        return configuration(device, DESCRIPTOR_CONFIGURATION, device->speed, setup);
    case DESCRIPTOR_STRING:
        return string_descriptor(device, index, setup);
    case DESCRIPTOR_DEVICE_QUALIFIER:
        return device->high_speed_allowed ? data(DEVICE_QUALIFIER, sizeof DEVICE_QUALIFIER, setup)
                                          : stall();
    case DESCRIPTOR_OTHER_SPEED_CONFIGURATION:
        return device->high_speed_allowed
                   ? configuration(device, DESCRIPTOR_OTHER_SPEED_CONFIGURATION,
                                   other_speed(device->speed), setup)
                   : stall();
    default:
        return stall();
    }
}

static UsbReply one_byte(UsbDevice *device, uint8_t value, UsbSetup setup)
{
    device->scratch[0] = value;
    return data(device->scratch, 1, setup);
}

static UsbReply two_zero_bytes(UsbDevice *device, UsbSetup setup)
{
    device->scratch[0] = 0;
    device->scratch[1] = 0;
    return data(device->scratch, 2, setup);
}

static UsbReply set_configuration(UsbDevice *device, uint16_t value)
{
    if (value > 1) {
        return stall();
    }
    device->configuration = (uint8_t)value;
    return status(USB_ACTION_CONFIGURE, value);
}

static UsbReply standard_in(UsbDevice *device, UsbSetup setup)
{
    switch (setup.request) {
    case REQUEST_GET_DESCRIPTOR:
        return setup.request_type == TYPE_STANDARD_DEVICE_IN ? descriptor(device, setup) : stall();
    case REQUEST_GET_CONFIGURATION:
        return one_byte(device, device->configuration, setup);
    case REQUEST_GET_INTERFACE:
        return one_byte(device, 0, setup);
    case REQUEST_GET_STATUS:
        return two_zero_bytes(device, setup);
    default:
        return stall();
    }
}

static UsbReply standard_out(UsbDevice *device, UsbSetup setup)
{
    switch (setup.request) {
    case REQUEST_SET_ADDRESS:
        return status(USB_ACTION_SET_ADDRESS, setup.value);
    case REQUEST_SET_CONFIGURATION:
        return set_configuration(device, setup.value);
    case REQUEST_SET_INTERFACE:
        return setup.value == 0 ? status(USB_ACTION_NONE, 0) : stall();
    case REQUEST_CLEAR_FEATURE:
        return setup.request_type == TYPE_STANDARD_ENDPOINT_OUT &&
                       setup.value == FEATURE_ENDPOINT_HALT
                   ? status(USB_ACTION_CLEAR_HALT, setup.index)
                   : status(USB_ACTION_NONE, 0);
    default:
        return stall();
    }
}

UsbReply usb_device_setup(UsbDevice *device, UsbSetup setup)
{
    switch (setup.request_type) {
    case TYPE_STANDARD_DEVICE_IN:
    case TYPE_STANDARD_INTERFACE_IN:
    case TYPE_STANDARD_ENDPOINT_IN:
        return standard_in(device, setup);
    case TYPE_STANDARD_DEVICE_OUT:
    case TYPE_STANDARD_INTERFACE_OUT:
    case TYPE_STANDARD_ENDPOINT_OUT:
        return standard_out(device, setup);
    case TYPE_CLASS_INTERFACE_IN:
        return setup.request == REQUEST_MSC_GET_MAX_LUN
                   ? one_byte(device, (uint8_t)(device->lun_count - 1), setup)
                   : stall();
    case TYPE_CLASS_INTERFACE_OUT:
        return setup.request == REQUEST_MSC_RESET ? status(USB_ACTION_MSC_RESET, 0) : stall();
    default:
        return stall();
    }
}

void usb_device_allow_high_speed(UsbDevice *device, bool allowed)
{
    device->high_speed_allowed = allowed;
}

void usb_device_set_speed(UsbDevice *device, UsbSpeed speed)
{
    device->speed = speed;
}

uint16_t usb_device_bulk_packet_size(const UsbDevice *device)
{
    return packet_size(device->speed);
}

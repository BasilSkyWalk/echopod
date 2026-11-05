#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/usb_device.h"

enum {
    USB_AUDIO_ENDPOINT = 0x01,
    USB_AUDIO_MAX_PACKET = 294,
    USB_AUDIO_DEFAULT_RATE = 48000,
    USB_AUDIO_CHANGED_FORMAT = 1,
    USB_AUDIO_CHANGED_VOLUME = 2,
    USB_AUDIO_MUTED = 0xFF,
    USB_HID_INTERFACE = 2,
    USB_HID_ENDPOINT = 0x82,
    USB_HID_ENDPOINT_NUMBER = 2,
    USB_HID_REPORT_LENGTH = 1,
    USB_HID_PACKET_SIZE = 8,
    USB_HID_INTERVAL_MS = 10,
    USB_HID_PLAY_PAUSE = 1,
    USB_HID_NEXT = 2,
    USB_HID_PREVIOUS = 4
};

typedef struct UsbAudio {
    UsbDevice *device;
    uint8_t alt;
    bool muted;
    uint8_t changes;
    int16_t volume;
    uint8_t report;
    uint32_t rate;
    uint8_t reply[4];
} UsbAudio;

void usb_audio_init(UsbAudio *audio, UsbDevice *device);
void usb_audio_reset(UsbAudio *audio);
UsbReply usb_audio_setup(UsbAudio *audio, UsbSetup setup);
UsbReply usb_audio_receive(UsbAudio *audio, UsbSetup setup, const uint8_t *data, uint16_t length);
uint8_t usb_audio_take_changes(UsbAudio *audio);
uint8_t usb_audio_sample_bytes(const UsbAudio *audio);
void usb_audio_set_report(UsbAudio *audio, uint8_t report);
uint8_t usb_audio_attenuation(const UsbAudio *audio);

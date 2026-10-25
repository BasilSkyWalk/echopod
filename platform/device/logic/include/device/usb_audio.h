#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/usb_device.h"

enum {
    USB_AUDIO_ENDPOINT = 0x01,
    USB_AUDIO_MAX_PACKET = 582,
    USB_AUDIO_DEFAULT_RATE = 48000,
    USB_AUDIO_CHANGED_FORMAT = 1,
    USB_AUDIO_CHANGED_VOLUME = 2,
    USB_AUDIO_MUTED = 0xFF,
    USB_AUDIO_EP0_FIFO_WORDS = 16
};

typedef struct UsbAudio {
    UsbDevice *device;
    uint8_t alt;
    bool muted;
    uint8_t changes;
    int16_t volume;
    uint32_t rate;
    uint8_t reply[4];
} UsbAudio;

typedef struct UsbFifoSizes {
    uint32_t rx;
    uint32_t tx0;
} UsbFifoSizes;

void usb_audio_init(UsbAudio *audio, UsbDevice *device);
void usb_audio_reset(UsbAudio *audio);
UsbReply usb_audio_setup(UsbAudio *audio, UsbSetup setup);
UsbReply usb_audio_receive(UsbAudio *audio, UsbSetup setup, const uint8_t *data, uint16_t length);
uint8_t usb_audio_take_changes(UsbAudio *audio);
uint8_t usb_audio_sample_bytes(const UsbAudio *audio);
uint8_t usb_audio_attenuation(const UsbAudio *audio);
UsbFifoSizes usb_audio_fifo_sizes(UsbFifoSizes reset);

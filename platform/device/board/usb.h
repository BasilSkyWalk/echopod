#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/msc.h"
#include "device/usb_audio.h"
#include "device/usb_device.h"

enum {
    USB_PORT_PACKET_BYTES = (int)USB_AUDIO_MAX_PACKET > (int)USB_HIGH_SPEED_PACKET_SIZE
        ? USB_AUDIO_MAX_PACKET
        : USB_HIGH_SPEED_PACKET_SIZE,
    USB_PORT_PACKET_WORDS = (USB_PORT_PACKET_BYTES + 3) / 4
};

typedef enum UsbConnectStep {
    USB_CONNECT_OK,
    USB_CONNECT_AHB_IDLE,
    USB_CONNECT_SOFT_RESET,
    USB_CONNECT_FLUSH
} UsbConnectStep;

typedef struct UsbProbe {
    uint32_t core_id;
    uint32_t otg_control;
    uint32_t hardware_3;
    uint32_t hardware_4;
    uint32_t interrupts;
    uint32_t rx_fifo_size;
    uint32_t tx_fifo_size;
} UsbProbe;

typedef struct UsbPort {
    UsbConnectStep connect_step;
    bool dedicated_fifos;
    bool configured;
    bool in_busy;
    bool setup_pending;
    bool high_speed_allowed;
    uint16_t bulk_packet;
    uint32_t setup[2];
    bool control_out_pending;
    UsbSetup control_out;
    uint32_t out_packet[USB_PORT_PACKET_WORDS];
    UsbDevice device;
    Msc *msc;
    UsbAudio *audio;
    void (*receive_stream)(const uint8_t *bytes, uint32_t length);
} UsbPort;

bool usb_vbus_present(void);
void usb_power_up(void);
void usb_power_down(void);
void usb_silence_audio(void);
bool usb_host_reset_pending(void);
bool usb_connect(UsbPort *port);
bool usb_wait_for_host(UsbPort *port, uint32_t timeout_ms);
void usb_poll(UsbPort *port);
UsbProbe usb_probe(void);
bool usb_reply_pending(const UsbPort *port);

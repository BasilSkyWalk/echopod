#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "emmc.h"
#include "hal/system.h"
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

typedef struct UsbShared {
    UsbLink link;
    UsbReport report;
    EmmcInfo internal;
    HalUsbLast last_session;
    bool high_speed_failed;
    bool flash_rewritten;
    bool dac_volume_known;
    uint8_t dac_volume;
} UsbShared;

typedef struct UsbStorageOutcome {
    bool cable_back;
    bool card_changed;
} UsbStorageOutcome;

typedef struct UsbOverlayEntry {
    uint32_t magic;
    UsbStorageOutcome (*storage)(UsbShared *shared);
    void (*dac)(UsbShared *shared);
} UsbOverlayEntry;

bool usb_session_listen(void);
bool usb_dac_session_listen(void);
bool usb_session_host_seen(void);
void usb_session_stop_listening(void);
const UsbReport *usb_session_report(void);
UsbShared *usb_session_shared(void);

UsbStorageOutcome usb_storage_session(UsbShared *shared);
void usb_dac_session(UsbShared *shared);
uint32_t *usb_session_buffer(void);
void update_after_usb(UsbShared *shared);

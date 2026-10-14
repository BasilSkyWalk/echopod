#include "usb_session.h"

#include <stddef.h>

#include "app/app.h"
#include "device/block_device.h"
#include "emmc.h"
#include "hal/clock.h"
#include "sd.h"
#include "usb.h"

enum {
    HOST_TIMEOUT_MS = 3000,
    LAST_REPLY_TIMEOUT_MS = 200,
    SERIAL_DIGITS = 8,
    INTERNAL_LUN = 0,
    CARD_LUN = 1,
    LUN_COUNT = 2
};

static EmmcInfo internal;
static UsbReport report;
static UsbPort port;
static Msc msc;

static bool card_write(void *context, uint32_t lba, uint32_t count, const void *buffer)
{
    (void)context;
    (void)lba;
    (void)count;
    (void)buffer;
    return false;
}

static void serial_text(uint32_t serial, char *text)
{
    static const char DIGITS[] = "0123456789ABCDEF";
    for (int index = 0; index < SERIAL_DIGITS; index++) {
        text[index] = DIGITS[(serial >> (28 - 4 * index)) & 0xFu];
    }
    text[SERIAL_DIGITS] = '\0';
}

static void prepare_luns(void)
{
    BlockDevice card;
    bool internal_ready = emmc_start(&internal);
    bool card_ready = sd_start(&card);
    MscLun luns[LUN_COUNT] = {
        {emmc_volume_read, emmc_volume_write, NULL, internal.layout.volume_sectors, true,
         internal_ready, "Internal storage"},
        {sd_read, card_write, NULL, sd_sector_count(), false, card_ready, "SD card"},
    };
    report.internal_ready = internal_ready;
    msc_init(&msc, luns, LUN_COUNT);
    char serial[SERIAL_DIGITS + 1];
    serial_text(internal.serial, serial);
    port = (UsbPort){.msc = &msc};
    usb_device_init(&port.device, serial, LUN_COUNT);
}

static void finish_last_reply(void)
{
    uint32_t start = hal_clock_milliseconds();
    while (usb_reply_pending(&port) && hal_clock_milliseconds() - start < LAST_REPLY_TIMEOUT_MS) {
        usb_poll(&port);
    }
}

static UsbSessionEnd serve(void)
{
    while (usb_vbus_present()) {
        hal_clock_milliseconds();
        usb_poll(&port);
        if (msc_ejected(&msc, INTERNAL_LUN)) {
            finish_last_reply();
            return USB_SESSION_EJECTED;
        }
    }
    return USB_SESSION_UNPLUGGED;
}

UsbSessionEnd usb_session_run(void)
{
    app_show_notice("USB", "Cable detected. Starting...");
    report = (UsbReport){.internal_ready = false};
    prepare_luns();
    usb_power_up();
    report.after_power_up = usb_probe();
    UsbSessionEnd end = USB_SESSION_FAILED;
    if (usb_connect(&port)) {
        end = USB_SESSION_NO_HOST;
        if (usb_wait_for_host(&port, HOST_TIMEOUT_MS)) {
            app_show_notice("USB", "Connected to a computer. Eject Internal storage to finish.");
            end = serve();
        }
    }
    report.connect_step = port.connect_step;
    report.after_wait = usb_probe();
    usb_power_down();
    return end;
}

const UsbReport *usb_session_report(void)
{
    return &report;
}

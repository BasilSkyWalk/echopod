#include "usb_session.h"

#include <stddef.h>

#include "app/app.h"
#include "core/string_builder.h"
#include "device/block_device.h"
#include "emmc.h"
#include "hal/clock.h"
#include "sd.h"
#include "usb.h"
#include "usb_report.h"

enum {
    HOST_TIMEOUT_MS = 3000,
    LAST_REPLY_TIMEOUT_MS = 200,
    COUNTER_REFRESH_MS = 1000,
    COUNTER_TEXT_CAPACITY = 48,
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

static void append_count(StringBuilder *builder, char label, uint32_t value)
{
    string_builder_append_char(builder, label);
    string_builder_append_unsigned(builder, value);
    string_builder_append_char(builder, ' ');
}

static void append_history(StringBuilder *line)
{
    MscHistory history = msc_history(&msc);
    for (uint32_t index = 0; index < history.count; index++) {
        MscHistoryEntry entry = history.entries[index];
        usb_report_append_hex(line, entry.opcode, 2);
        string_builder_append_unsigned(line, entry.status);
        string_builder_append_char(line, entry.lun == 0 ? ' ' : '+');
    }
}

static void show_counters(MscStats stats)
{
    char title[COUNTER_TEXT_CAPACITY];
    char message[COUNTER_TEXT_CAPACITY];
    StringBuilder top = string_builder_make(title, sizeof title);
    string_builder_append(&top, "USB ");
    append_count(&top, 'R', stats.sectors_read);
    append_count(&top, 'W', stats.sectors_written);
    append_count(&top, 'E', stats.read_failures + stats.write_failures);
    StringBuilder line = string_builder_make(message, sizeof message);
    append_history(&line);
    app_show_notice(title, message);
}

static bool stats_changed(MscStats now, MscStats shown)
{
    return now.write_failures != shown.write_failures || now.read_failures != shown.read_failures;
}

static UsbSessionEnd serve(void)
{
    MscStats shown = msc_stats(&msc);
    uint32_t refreshed = hal_clock_milliseconds();
    while (usb_vbus_present()) {
        uint32_t now = hal_clock_milliseconds();
        if (now - refreshed >= COUNTER_REFRESH_MS && stats_changed(msc_stats(&msc), shown)) {
            shown = msc_stats(&msc);
            show_counters(shown);
            refreshed = hal_clock_milliseconds();
        }
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

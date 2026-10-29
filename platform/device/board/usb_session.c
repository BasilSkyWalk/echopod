#include "usb_session.h"

#include <stddef.h>

#include "app/app.h"
#include "board.h"
#include "core/string_builder.h"
#include "device/block_device.h"
#include "device/usb_fallback.h"
#include "emmc.h"
#include "hal/clock.h"
#include "hal/system.h"
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
    LUN_COUNT = 2,
    WRITE_BUFFER_SECTORS = 64,
    WORKING_COMMANDS = 2
};

static EmmcInfo internal;
static bool card_present;
static UsbReport report;
static UsbPort port;
static Msc msc;
static UsbFallback fallback;
static bool high_speed_failed;
static HalUsbLast last_session;
static uint32_t write_buffer[WRITE_BUFFER_SECTORS * MSC_SECTOR_SIZE / 4]
    __attribute__((section(".high_ram")));

static void serial_text(uint32_t serial, char *text)
{
    static const char DIGITS[] = "0123456789ABCDEF";
    for (int index = 0; index < SERIAL_DIGITS; index++) {
        text[index] = DIGITS[(serial >> (28 - 4 * index)) & 0xFu];
    }
    text[SERIAL_DIGITS] = '\0';
}

static void prepare_port(void)
{
    report.internal_ready = emmc_start(&internal);
    char serial[SERIAL_DIGITS + 1];
    serial_text(internal.serial, serial);
    port = (UsbPort){.msc = &msc, .high_speed_allowed = !high_speed_failed};
    usb_device_init(&port.device, serial, LUN_COUNT);
    usb_device_allow_high_speed(&port.device, port.high_speed_allowed);
}

static void prepare_luns(void)
{
    BlockDevice card;
    bool card_ready = sd_start(&card);
    MscLun luns[LUN_COUNT] = {
        {emmc_volume_read, emmc_volume_write, NULL, internal.layout.volume_sectors, true,
         report.internal_ready, "Internal storage"},
        {sd_read, sd_write, NULL, sd_sector_count(), true, card_ready, "SD card"},
    };
    card_present = card_ready;
    msc_init(&msc, luns, LUN_COUNT);
    msc_use_write_buffer(&msc, write_buffer, WRITE_BUFFER_SECTORS);
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

static bool session_works(void)
{
    return port.configured && msc_stats(&msc).commands >= WORKING_COMMANDS;
}

static void fall_back_to_full_speed(void)
{
    high_speed_failed = true;
    port.high_speed_allowed = false;
    port.configured = false;
    port.in_busy = false;
    usb_device_allow_high_speed(&port.device, false);
    msc_reset(&msc);
    usb_connect(&port);
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
        board_display_service();
        if (usb_fallback_due(&fallback, session_works(), hal_clock_milliseconds())) {
            fall_back_to_full_speed();
        }
        if (msc_finished(&msc)) {
            finish_last_reply();
            return USB_SESSION_EJECTED;
        }
    }
    return USB_SESSION_UNPLUGGED;
}

static HalUsbEnd hal_end(UsbSessionEnd end)
{
    if (end == USB_SESSION_EJECTED) {
        return HAL_USB_END_EJECTED;
    }
    return end == USB_SESSION_UNPLUGGED ? HAL_USB_END_UNPLUGGED : HAL_USB_END_NO_HOST;
}

static void remember_session(UsbSessionEnd end)
{
    MscStats stats = msc_stats(&msc);
    last_session = (HalUsbLast){.end = hal_end(end),
                                .left_mounted = msc_left_mounted(&msc),
                                .internal_written = stats.lun_sectors_written[INTERNAL_LUN],
                                .card_written = stats.lun_sectors_written[CARD_LUN],
                                .writes_after_eject = stats.writes_after_eject};
}

bool usb_session_listen(void)
{
    report = (UsbReport){.internal_ready = false};
    prepare_port();
    usb_power_up();
    report.after_power_up = usb_probe();
    bool connected = usb_connect(&port);
    report.connect_step = port.connect_step;
    if (!connected) {
        usb_power_down();
    }
    return connected;
}

bool usb_session_host_seen(void)
{
    return usb_host_reset_pending();
}

void usb_session_stop_listening(void)
{
    report.after_wait = usb_probe();
    usb_power_down();
}

UsbSessionEnd usb_session_run(void)
{
    app_show_notice("USB", "Computer found. Starting...");
    prepare_luns();
    app_release_audio();
    board_audio_shut_down();
    usb_silence_audio();
    UsbSessionEnd end = USB_SESSION_NO_HOST;
    if (usb_wait_for_host(&port, HOST_TIMEOUT_MS)) {
        usb_fallback_start(&fallback, port.high_speed_allowed, hal_clock_milliseconds());
        app_show_notice("USB", card_present ? "Connected. Eject both drives to finish."
                                            : "Connected. Eject the drive to finish.");
        end = serve();
    }
    usb_session_stop_listening();
    remember_session(end);
    return end;
}

HalUsbLast hal_system_usb_last(void)
{
    return last_session;
}

const UsbReport *usb_session_report(void)
{
    return &report;
}

bool usb_session_card_changed(void)
{
    return msc_stats(&msc).lun_sectors_written[CARD_LUN] > 0;
}

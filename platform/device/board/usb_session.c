#include "usb_session.h"

#include <stddef.h>

#include "app/app.h"
#include "board.h"
#include "core/i18n.h"
#include "core/string_builder.h"
#include "device/block_device.h"
#include "device/usb_fallback.h"
#include "device/vbus_debounce.h"
#include "emmc.h"
#include "hal/clock.h"
#include "hal/display.h"
#include "hal/input.h"
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
    WORKING_COMMANDS = 2,
    SCREEN_DARK_MS = 30000
};

static UsbShared *shared;
static bool card_present;
static UsbPort port;
static Msc msc;
static UsbFallback fallback;
static VbusDebounce vbus;
static SdHealth card_at_start;
static bool screen_dark;
static uint32_t screen_used_ms;
static uint32_t write_buffer[WRITE_BUFFER_SECTORS * MSC_SECTOR_SIZE / 4];

uint32_t *usb_session_buffer(void)
{
    return write_buffer;
}

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
    char serial[SERIAL_DIGITS + 1];
    serial_text(shared->internal.serial, serial);
    port = (UsbPort){.link = shared->link, .msc = &msc, .bulk_packet = USB_PACKET_SIZE};
    usb_device_init(&port.device, serial, LUN_COUNT);
    usb_device_allow_high_speed(&port.device, port.link.high_speed_allowed);
}

static void prepare_luns(void)
{
    BlockDevice card;
    card_at_start = sd_health();
    bool card_ready = sd_prepare(&card);
    MscLun luns[LUN_COUNT] = {
        {emmc_volume_read, emmc_volume_write, NULL, shared->internal.layout.volume_sectors, true,
         shared->report.internal_ready, "Internal storage"},
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

static void light_screen(void)
{
    screen_used_ms = hal_clock_milliseconds();
    if (screen_dark) {
        screen_dark = false;
        hal_display_set_power(true);
    }
}

static void dim_when_idle(void)
{
    HalKeyTransition transition;
    bool pressed = false;
    while (hal_input_next_transition(&transition)) {
        pressed = pressed || transition.pressed;
    }
    if (pressed) {
        light_screen();
    } else if (!screen_dark && hal_clock_milliseconds() - screen_used_ms >= SCREEN_DARK_MS) {
        screen_dark = true;
        hal_display_set_power(false);
    }
}

static void show_counters(MscStats stats)
{
    light_screen();
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
    shared->high_speed_failed = true;
    port.link.high_speed_allowed = false;
    port.configured = false;
    port.in_busy = false;
    usb_device_allow_high_speed(&port.device, false);
    msc_reset(&msc);
    usb_port_connect(&port);
}

static UsbSessionEnd serve(void)
{
    MscStats shown = msc_stats(&msc);
    uint32_t refreshed = hal_clock_milliseconds();
    light_screen();
    while (vbus_debounce_present(&vbus, usb_vbus_present(), hal_clock_milliseconds())) {
        uint32_t now = hal_clock_milliseconds();
        if (now - refreshed >= COUNTER_REFRESH_MS && stats_changed(msc_stats(&msc), shown)) {
            shown = msc_stats(&msc);
            show_counters(shown);
            refreshed = hal_clock_milliseconds();
        }
        usb_poll(&port);
        board_display_service();
        dim_when_idle();
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
    SdHealth card = sd_health();
    bool card_failed = card.failures != card_at_start.failures;
    shared->last_session =
        (HalUsbLast){.end = hal_end(end),
                     .left_mounted = msc_left_mounted(&msc),
                     .internal_written = stats.lun_sectors_written[INTERNAL_LUN],
                     .card_written = stats.lun_sectors_written[CARD_LUN],
                     .writes_after_eject = stats.writes_after_eject,
                     .internal_errors = stats.lun_failures[INTERNAL_LUN],
                     .card_errors = stats.lun_failures[CARD_LUN],
                     .card_retries = card.retry.retries - card_at_start.retry.retries,
                     .card_starts = card.starts - card_at_start.starts,
                     .card_step = card_failed ? (uint8_t)card.failure.step : 0,
                     .card_interrupts = card_failed ? card.failure.interrupts : 0,
                     .host_resets = port.configured_resets,
                     .vbus_dips = vbus.dips,
                     .fell_back = fallback.fired,
                     .full_speed = !port.link.high_speed_allowed};
}

static UsbSessionEnd run(void)
{
    app_show_notice("USB", i18n_text(STR_USB_COMPUTER_FOUND));
    vbus_debounce_start(&vbus);
    fallback = (UsbFallback){.watching = false};
    prepare_port();
    prepare_luns();
    usb_silence_audio();
    UsbSessionEnd end = USB_SESSION_NO_HOST;
    if (usb_wait_for_host(&port, HOST_TIMEOUT_MS)) {
        usb_fallback_start(&fallback, port.link.high_speed_allowed, hal_clock_milliseconds());
        app_show_notice("USB", i18n_text(card_present ? STR_USB_CONNECTED_EJECT_BOTH
                                                      : STR_USB_CONNECTED_EJECT_ONE));
        end = serve();
        light_screen();
    }
    usb_session_stop_listening();
    remember_session(end);
    return end;
}

static bool card_changed(void)
{
    return msc_stats(&msc).lun_sectors_written[CARD_LUN] > 0 ||
           sd_health().starts != card_at_start.starts;
}

UsbStorageOutcome usb_storage_session(UsbShared *state)
{
    shared = state;
    UsbSessionEnd end = run();
    bool cable_back = end == USB_SESSION_UNPLUGGED && usb_vbus_present();
    if (end == USB_SESSION_UNPLUGGED && !cable_back && shared->last_session.left_mounted != 0) {
        app_show_notice(i18n_text(STR_USB_UNPLUGGED_TITLE), i18n_text(STR_USB_UNPLUGGED_MESSAGE));
        board_wait_for_key();
    }
    if (end == USB_SESSION_EJECTED || end == USB_SESSION_UNPLUGGED) {
        update_after_usb(shared);
    }
    return (UsbStorageOutcome){.cable_back = cable_back, .card_changed = card_changed()};
}

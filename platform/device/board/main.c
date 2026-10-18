#include <stdbool.h>

#include "app/app.h"
#include "board.h"
#include "hal/clock.h"
#include "hal/system.h"
#include "storage_reset.h"
#include "update.h"
#include "usb.h"
#include "usb_report.h"
#include "usb_session.h"

enum {
    START_CLOCK_MHZ = 96,
    HOST_WAIT_MS = 3000
};

typedef enum UsbState {
    USB_STATE_UNPLUGGED,
    USB_STATE_LISTENING,
    USB_STATE_HANDLED
} UsbState;

static UsbState usb_state;
static uint32_t listen_started_ms;

bool board_usb_listening(void)
{
    return usb_state == USB_STATE_LISTENING;
}

static void raise_clock_for_usb(void)
{
    if (hal_system_clock_mhz() < START_CLOCK_MHZ) {
        hal_system_set_clock_mhz(START_CLOCK_MHZ);
    }
}

static void run_usb_session(void)
{
    raise_clock_for_usb();
    UsbSessionEnd end = usb_session_run();
    if (end == USB_SESSION_EJECTED || end == USB_SESSION_UNPLUGGED) {
        update_after_usb();
    }
    if (usb_session_card_changed()) {
        storage_reset();
        app_start();
    }
    app_wake();
}

static void start_listening(void)
{
    raise_clock_for_usb();
    if (!usb_session_listen()) {
        usb_report_show(usb_session_report());
        app_wake();
        usb_state = USB_STATE_HANDLED;
        return;
    }
    listen_started_ms = hal_clock_milliseconds();
    usb_state = USB_STATE_LISTENING;
}

static void keep_listening(void)
{
    if (usb_session_host_seen()) {
        usb_state = USB_STATE_HANDLED;
        run_usb_session();
    } else if (hal_clock_milliseconds() - listen_started_ms >= HOST_WAIT_MS) {
        usb_session_stop_listening();
        usb_state = USB_STATE_HANDLED;
    }
}

static void check_usb(void)
{
    bool vbus = usb_vbus_present();
    if (!vbus) {
        if (usb_state == USB_STATE_LISTENING) {
            usb_session_stop_listening();
        }
        usb_state = USB_STATE_UNPLUGGED;
    } else if (usb_state == USB_STATE_UNPLUGGED) {
        start_listening();
    } else if (usb_state == USB_STATE_LISTENING) {
        keep_listening();
    }
}

void board_main(void)
{
    board_start_record_boot();
    board_power_hold();
    board_bluetooth_off();
    board_clock_record_boot();
    board_clock_start();
    hal_system_set_clock_mhz(START_CLOCK_MHZ);
    board_input_start();
    board_display_start();
    app_start();
    app_show_splash();
    for (;;) {
        app_step();
        check_usb();
    }
}

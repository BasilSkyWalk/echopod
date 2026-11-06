#include <stdbool.h>

#include "app/app.h"
#include "board.h"
#include "core/i18n.h"
#include "device/battery_gauge.h"
#include "device/sleep_meter.h"
#include "hal/clock.h"
#include "hal/power.h"
#include "hal/system.h"
#include "storage_reset.h"
#include "update.h"
#include "usb.h"
#include "usb_dac_session.h"
#include "usb_report.h"
#include "usb_session.h"

enum {
    START_CLOCK_MHZ = 96,
    HOST_WAIT_MS = 3000,
    ASK_TIMEOUT_MS = 5000,
    EMPTY_NOTICE_MS = 2000,
    USB_CHOICE_STORAGE = 0,
    USB_CHOICE_DAC = 1,
    USB_CHOICE_CHARGE_ONLY = 2,
    USB_CHOICE_COUNT = 3
};

typedef enum UsbState {
    USB_STATE_UNPLUGGED,
    USB_STATE_LISTENING,
    USB_STATE_ASKING,
    USB_STATE_HANDLED
} UsbState;

static const StringId USB_CHOICES[USB_CHOICE_COUNT] = {STR_SETTINGS_STORAGE, STR_SETTINGS_DAC,
                                                       STR_USB_CHARGE_ONLY};

static UsbState usb_state;
static uint32_t listen_started_ms;
static uint32_t ask_started_ms;
static HalUsbMode usb_mode;
static bool usb_prompt;
static bool dac_session;
static SleepMeter sleep_meter;

void hal_system_set_usb_mode(HalUsbMode mode)
{
    usb_mode = mode;
}

void hal_system_set_usb_prompt(bool ask)
{
    usb_prompt = ask;
}

static int saved_choice(void)
{
    return usb_mode == HAL_USB_MODE_DAC ? USB_CHOICE_DAC : USB_CHOICE_STORAGE;
}

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
    board_stall_pause(true);
    raise_clock_for_usb();
    UsbSessionEnd end = usb_session_run();
    if (end == USB_SESSION_UNPLUGGED && hal_system_usb_last().left_mounted != 0) {
        app_show_notice(i18n_text(STR_USB_UNPLUGGED_TITLE), i18n_text(STR_USB_UNPLUGGED_MESSAGE));
        board_wait_for_key();
    }
    if (end == USB_SESSION_EJECTED || end == USB_SESSION_UNPLUGGED) {
        update_after_usb();
    }
    if (usb_session_card_changed()) {
        storage_reset();
        app_start();
        app_request_rescan_if_music_changed();
    }
    app_wake();
    board_stall_pause(false);
}

static void run_dac_session(void)
{
    board_stall_pause(true);
    raise_clock_for_usb();
    usb_dac_session_run();
    app_wake();
    board_stall_pause(false);
}

static void run_session(void)
{
    if (dac_session) {
        run_dac_session();
    } else {
        run_usb_session();
    }
}

static void stop_listening(void)
{
    if (dac_session) {
        usb_power_down();
    } else {
        usb_session_stop_listening();
    }
}

static void report_failed_start(void)
{
    if (!dac_session) {
        board_stall_pause(true);
        usb_report_show(usb_session_report());
        board_stall_pause(false);
        app_wake();
    }
}

static bool listen(int choice)
{
    raise_clock_for_usb();
    dac_session = choice == USB_CHOICE_DAC;
    if (dac_session ? usb_dac_session_listen() : usb_session_listen()) {
        return true;
    }
    report_failed_start();
    return false;
}

static void start_listening(void)
{
    if (!listen(saved_choice())) {
        usb_state = USB_STATE_HANDLED;
        return;
    }
    listen_started_ms = hal_clock_milliseconds();
    usb_state = USB_STATE_LISTENING;
}

static void start_asking(void)
{
    stop_listening();
    app_choice_show(i18n_text(STR_USB_PROMPT_TITLE), USB_CHOICES, USB_CHOICE_COUNT, saved_choice());
    ask_started_ms = hal_clock_milliseconds();
    usb_state = USB_STATE_ASKING;
}

static void keep_listening(void)
{
    if (usb_session_host_seen()) {
        if (usb_prompt) {
            start_asking();
        } else {
            usb_state = USB_STATE_HANDLED;
            run_session();
        }
    } else if (hal_clock_milliseconds() - listen_started_ms >= HOST_WAIT_MS) {
        stop_listening();
        usb_state = USB_STATE_HANDLED;
    }
}

static void start_chosen_session(int choice)
{
    if (choice == USB_CHOICE_CHARGE_ONLY) {
        app_wake();
    } else if (listen(choice)) {
        run_session();
    }
}

static void keep_asking(void)
{
    int choice = app_choice_step();
    bool timed_out = hal_clock_milliseconds() - ask_started_ms >= ASK_TIMEOUT_MS;
    if (choice < 0 && !timed_out) {
        return;
    }
    app_choice_close();
    usb_state = USB_STATE_HANDLED;
    start_chosen_session(choice < 0 ? saved_choice() : choice);
}

static void check_usb(void)
{
    bool vbus = usb_vbus_present();
    if (!vbus) {
        if (usb_state == USB_STATE_LISTENING) {
            stop_listening();
        } else if (usb_state == USB_STATE_ASKING) {
            app_choice_close();
        }
        usb_state = USB_STATE_UNPLUGGED;
    } else if (usb_state == USB_STATE_UNPLUGGED) {
        start_listening();
    } else if (usb_state == USB_STATE_LISTENING) {
        keep_listening();
    } else if (usb_state == USB_STATE_ASKING) {
        keep_asking();
    }
}

static void switch_off_when_empty(uint32_t boot_millivolts)
{
    if (!battery_boot_cutoff(boot_millivolts, usb_vbus_present())) {
        return;
    }
    app_show_notice("Battery low", "Charge the player to turn it on");
    board_delay_ms(EMPTY_NOTICE_MS);
    hal_power_off();
}

static void sleep_while_idle(void)
{
    sleep_meter_advance(&sleep_meter, hal_clock_microseconds());
    if (!board_display_lit() || app_idle()) {
        uint32_t before_us = hal_clock_microseconds();
        __asm__ volatile("wfi" ::: "memory");
        sleep_meter_slept(&sleep_meter, hal_clock_microseconds() - before_us);
    }
}

uint8_t board_sleep_percent(void)
{
    return sleep_meter_percent(&sleep_meter);
}

void board_main(void)
{
    board_start_record_boot();
    board_power_hold();
    board_bluetooth_off();
    board_clock_record_boot();
    board_clock_start();
    sleep_meter_init(&sleep_meter, hal_clock_microseconds());
    board_stall_pause(true);
    hal_system_set_clock_mhz(START_CLOCK_MHZ);
    board_watchdog_start();
    uint32_t boot_millivolts = battery_millivolts(board_battery_boot_reading());
    board_input_start();
    board_display_start();
    board_glyphs_start();
    switch_off_when_empty(boot_millivolts);
    app_start();
    app_show_splash();
    board_stall_pause(false);
    for (;;) {
        board_stall_beat();
        if (usb_state != USB_STATE_ASKING) {
            app_step();
        }
        board_display_service();
        check_usb();
        sleep_while_idle();
    }
}

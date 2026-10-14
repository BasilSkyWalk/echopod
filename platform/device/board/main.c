#include <stdbool.h>

#include "app/app.h"
#include "board.h"
#include "storage_reset.h"
#include "update.h"
#include "usb.h"
#include "usb_report.h"
#include "usb_session.h"

static bool usb_handled;

static void check_usb(void)
{
    bool vbus = usb_vbus_present();
    if (vbus && !usb_handled) {
        usb_handled = true;
        UsbSessionEnd end = usb_session_run();
        if (end == USB_SESSION_EJECTED || end == USB_SESSION_UNPLUGGED) {
            update_after_usb();
        } else {
            usb_report_show(usb_session_report());
        }
        if (usb_session_card_changed()) {
            storage_reset();
            app_start();
        }
        app_wake();
    } else if (!vbus) {
        usb_handled = false;
    }
}

void board_main(void)
{
    board_clock_record_boot();
    board_clock_start();
    board_input_start();
    board_display_start();
    app_start();
    app_show_splash();
    for (;;) {
        app_step();
        check_usb();
    }
}

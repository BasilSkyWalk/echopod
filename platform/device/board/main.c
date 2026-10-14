#include <stdbool.h>

#include "app/app.h"
#include "board.h"
#include "usb.h"
#include "usb_session.h"

static bool usb_handled;

static void check_usb(void)
{
    bool vbus = usb_vbus_present();
    if (vbus && !usb_handled) {
        usb_handled = true;
        usb_session_run();
        app_wake();
    } else if (!vbus) {
        usb_handled = false;
    }
}

void board_main(void)
{
    board_clock_start();
    board_input_start();
    board_display_start();
    app_start();
    for (;;) {
        app_step();
        check_usb();
    }
}

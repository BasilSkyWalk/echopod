#include <stdbool.h>
#include <stdint.h>

#include "emmc.h"
#include "hal/system.h"
#include "usb.h"
#include "usb_session.h"

static UsbShared shared;

UsbShared *usb_session_shared(void)
{
    return &shared;
}

bool usb_session_listen(void)
{
    shared.report = (UsbReport){.internal_ready = emmc_start(&shared.internal)};
    shared.link = (UsbLink){.high_speed_allowed = !shared.high_speed_failed};
    usb_power_up();
    shared.report.after_power_up = usb_probe();
    bool connected = usb_connect(&shared.link);
    shared.report.connect_step = shared.link.step;
    if (!connected) {
        usb_power_down();
    }
    return connected;
}

bool usb_dac_session_listen(void)
{
    shared.link = (UsbLink){.high_speed_allowed = false};
    usb_power_up();
    if (usb_connect(&shared.link)) {
        return true;
    }
    usb_power_down();
    return false;
}

bool usb_session_host_seen(void)
{
    return usb_host_reset_pending();
}

void usb_session_stop_listening(void)
{
    shared.report.after_wait = usb_probe();
    usb_power_down();
}

const UsbReport *usb_session_report(void)
{
    return &shared.report;
}

HalUsbLast hal_system_usb_last(void)
{
    return shared.last_session;
}

bool hal_system_restart_pending(void)
{
    return shared.flash_rewritten;
}

void hal_system_set_dac_volume(uint8_t level)
{
    shared.dac_volume = level;
    shared.dac_volume_known = true;
}

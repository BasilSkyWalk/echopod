#include "diagnostics_rows.h"

#include "diagnostics_values.h"
#include "hal/audio.h"
#include "hal/power.h"
#include "hal/system.h"

const char *diagnostics_row_firmware(const void *context)
{
    (void)context;
    return hal_system_version();
}

static char battery_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_battery(const void *context)
{
    (void)context;
    diagnostics_battery_text(hal_power_battery(), battery_text, sizeof battery_text);
    return battery_text;
}

static char jack_single_ended_text[DIAGNOSTICS_VALUE_CAPACITY];
static char jack_balanced_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_jack_single_ended(const void *context)
{
    (void)context;
    diagnostics_jack_pin_text(hal_audio_jack_pins().single_ended, jack_single_ended_text,
                              sizeof jack_single_ended_text);
    return jack_single_ended_text;
}

const char *diagnostics_row_jack_balanced(const void *context)
{
    (void)context;
    diagnostics_jack_pin_text(hal_audio_jack_pins().balanced, jack_balanced_text,
                              sizeof jack_balanced_text);
    return jack_balanced_text;
}

static char start_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_start(const void *context)
{
    (void)context;
    diagnostics_start_text(hal_system_start_info(), start_text, sizeof start_text);
    return start_text;
}

static char last_step_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_last_step(const void *context)
{
    (void)context;
    diagnostics_last_step_text(hal_system_start_info(), last_step_text, sizeof last_step_text);
    return last_step_text;
}

static char last_battery_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_last_battery(const void *context)
{
    (void)context;
    diagnostics_last_battery_text(hal_system_start_info(), last_battery_text,
                                  sizeof last_battery_text);
    return last_battery_text;
}

static char usb_last_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_usb_last(const void *context)
{
    (void)context;
    diagnostics_usb_last_text(hal_system_usb_last(), usb_last_text, sizeof usb_last_text);
    return usb_last_text;
}

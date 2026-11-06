#include "diagnostics_rows.h"

#include <stddef.h>

#include "diagnostics_values.h"
#include "hal/audio.h"
#include "hal/display.h"
#include "hal/power.h"
#include "hal/system.h"
#include "hal/time.h"

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

static char panel_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_panel(const void *context)
{
    (void)context;
    diagnostics_panel_text(hal_display_panel_id(), panel_text, sizeof panel_text);
    return panel_text;
}

static HalTime clock_time;
static bool clock_read;
static char clock_text[DIAGNOSTICS_VALUE_CAPACITY];

ScreenAction diagnostics_row_clock_read(void *context)
{
    (void)context;
    hal_time_read(&clock_time);
    clock_read = true;
    return screen_action_none();
}

const char *diagnostics_row_clock(const void *context)
{
    (void)context;
    if (!clock_read) {
        diagnostics_row_clock_read(NULL);
    }
    diagnostics_time_text(clock_time, clock_text, sizeof clock_text);
    return clock_text;
}

static char power_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_power(const void *context)
{
    (void)context;
    diagnostics_power_text(hal_system_power_info(), power_text, sizeof power_text);
    return power_text;
}

static char savings_text[DIAGNOSTICS_VALUE_CAPACITY];

ScreenAction diagnostics_row_savings_next(void *context)
{
    (void)context;
    hal_system_set_power_savings(diagnostics_next_savings(hal_system_power_savings()));
    return screen_action_none();
}

const char *diagnostics_row_savings(const void *context)
{
    (void)context;
    diagnostics_savings_text(hal_system_power_savings(), savings_text, sizeof savings_text);
    return savings_text;
}

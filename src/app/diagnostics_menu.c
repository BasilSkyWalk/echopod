#include "app_screens.h"

#include <stdbool.h>
#include <stddef.h>

#include "diagnostics_rows.h"
#include "diagnostics_values.h"
#include "hal/power.h"
#include "hal/system.h"

static ScreenAction toggle_frame_time(void *context)
{
    AppOptions *options = context;
    options->show_frame_time = !options->show_frame_time;
    return screen_action_none();
}

static const char *frame_time_value(const void *context)
{
    const AppOptions *options = context;
    return options->show_frame_time ? "On" : "Off";
}

static ScreenAction toggle_power_taps(void *context)
{
    AppOptions *options = context;
    options->ignore_power_taps = !options->ignore_power_taps;
    return screen_action_none();
}

static const char *power_taps_value(const void *context)
{
    const AppOptions *options = context;
    return options->ignore_power_taps ? "Ignored" : "Normal";
}

static char clock_text[DIAGNOSTICS_VALUE_CAPACITY];

static ScreenAction cycle_clock(void *context)
{
    ClockOption *clock = context;
    clock->failed = !hal_system_set_clock_mhz(diagnostics_next_clock_mhz(hal_system_clock_mhz()));
    return screen_action_none();
}

static const char *clock_value(const void *context)
{
    const ClockOption *clock = context;
    diagnostics_clock_text(hal_system_clock_mhz(), clock->failed, clock_text, sizeof clock_text);
    return clock_text;
}

static char core1_text[DIAGNOSTICS_VALUE_CAPACITY];

static ScreenAction run_core1_test(void *context)
{
    Core1Option *core1 = context;
    core1->test = hal_system_core1_test();
    core1->run = true;
    return screen_action_none();
}

static const char *core1_value(const void *context)
{
    const Core1Option *core1 = context;
    diagnostics_core1_text(core1->run, core1->test, core1_text, sizeof core1_text);
    return core1_text;
}

static char core1_accel_text[DIAGNOSTICS_VALUE_CAPACITY];

static ScreenAction run_core1_accel_test(void *context)
{
    Core1AccelOption *accel = context;
    accel->result = hal_system_core1_accel_test();
    accel->run = true;
    return screen_action_none();
}

static const char *core1_accel_value(const void *context)
{
    const Core1AccelOption *accel = context;
    diagnostics_core1_accel_text(accel->run, accel->result, core1_accel_text,
                                 sizeof core1_accel_text);
    return core1_accel_text;
}

static char boot_hold_text[DIAGNOSTICS_VALUE_CAPACITY];

static const char *boot_hold_value(const void *context)
{
    (void)context;
    diagnostics_boot_hold_text(hal_power_boot_hold(), boot_hold_text, sizeof boot_hold_text);
    return boot_hold_text;
}

static MenuItem value_item(const char *label, const char *(*value)(const void *context))
{
    return (MenuItem){.label = label, .select = NULL, .context = NULL, .value = value};
}

static MenuItem action_item(const char *label, ScreenAction (*select)(void *context), void *context,
                            const char *(*value)(const void *context))
{
    return (MenuItem){.label = label, .select = select, .context = context, .value = value};
}

void diagnostics_menu_build(AppScreens *screens, AppOptions *options, const Theme *theme)
{
    MenuItem *items = screens->diagnostics_items;
    screens->clock = (ClockOption){.failed = false};
    screens->core1 = (Core1Option){.run = false};
    screens->core1_accel = (Core1AccelOption){.run = false};
    int count = 0;
    items[count++] =
        app_screens_open_item("Key test", test_pattern_screen_init(&screens->test_pattern));
    items[count++] = action_item("Frame time", toggle_frame_time, options, frame_time_value);
    items[count++] = value_item("Firmware", diagnostics_row_firmware);
    items[count++] = action_item("CPU clock", cycle_clock, &screens->clock, clock_value);
    items[count++] = action_item("Core 1", run_core1_test, &screens->core1, core1_value);
    items[count++] = value_item("Battery", diagnostics_row_battery);
    items[count++] = value_item("Boot hold", boot_hold_value);
    items[count++] = value_item("Last start", diagnostics_row_start);
    items[count++] = value_item("Last step", diagnostics_row_last_step);
    items[count++] = value_item("Last battery", diagnostics_row_last_battery);
    items[count++] = (MenuItem){.label = "Bluetooth",
                                .select = bluetooth_screen_open_details,
                                .context = &screens->bluetooth,
                                .value = NULL,
                                .opens = true};
    items[count++] = action_item("Power taps", toggle_power_taps, options, power_taps_value);
    items[count++] = value_item("Jack 3.5", diagnostics_row_jack_single_ended);
    items[count++] = value_item("Jack 4.4", diagnostics_row_jack_balanced);
    items[count++] =
        action_item("LPC accel", run_core1_accel_test, &screens->core1_accel, core1_accel_value);
    items[count++] = value_item("Last USB", diagnostics_row_usb_last);
    app_screens_build_menu(&screens->diagnostics, "Diagnostics", items, count, theme);
}

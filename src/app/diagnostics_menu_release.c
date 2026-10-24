#include "app_screens.h"

#include <stddef.h>

#include "diagnostics_rows.h"

enum {
    RELEASE_DIAGNOSTICS_COUNT = 9
};

static MenuItem value_item(const char *label, const char *(*value)(const void *context))
{
    return (MenuItem){.label = label, .select = NULL, .context = NULL, .value = value};
}

void diagnostics_menu_build(AppScreens *screens, AppOptions *options, const Theme *theme)
{
    (void)options;
    MenuItem *items = screens->diagnostics_items;
    items[0] = app_screens_open_item("Key test", test_pattern_screen_init(&screens->test_pattern));
    items[1] = value_item("Firmware", diagnostics_row_firmware);
    items[2] = value_item("Battery", diagnostics_row_battery);
    items[3] = value_item("Last start", diagnostics_row_start);
    items[4] = value_item("Last step", diagnostics_row_last_step);
    items[5] = value_item("Last battery", diagnostics_row_last_battery);
    items[6] = value_item("Last USB", diagnostics_row_usb_last);
    items[7] = value_item("Jack 3.5", diagnostics_row_jack_single_ended);
    items[8] = value_item("Jack 4.4", diagnostics_row_jack_balanced);
    app_screens_build_menu(&screens->diagnostics, "Diagnostics", items, RELEASE_DIAGNOSTICS_COUNT,
                           theme);
}

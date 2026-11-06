#include "app_screens.h"

#include <stddef.h>

#include "core/i18n.h"
#include "diagnostics_rows.h"

enum {
    RELEASE_DIAGNOSTICS_COUNT = 17
};

static MenuItem value_item(StringId label, const char *(*value)(const void *context))
{
    return (MenuItem){.label = i18n_text(label), .select = NULL, .context = NULL, .value = value};
}

static MenuItem action_item(StringId label, ScreenAction (*select)(void *context),
                            const char *(*value)(const void *context))
{
    return (MenuItem){.label = i18n_text(label), .select = select, .context = NULL, .value = value};
}

void diagnostics_menu_build(AppScreens *screens, AppOptions *options, const Theme *theme)
{
    (void)options;
    MenuItem *items = screens->diagnostics_items;
    items[0] = app_screens_open_item(i18n_text(STR_DIAG_KEY_TEST),
                                     test_pattern_screen_init(&screens->test_pattern));
    items[1] = value_item(STR_DIAG_FIRMWARE, diagnostics_row_firmware);
    items[2] = value_item(STR_DIAG_BATTERY, diagnostics_row_battery);
    items[3] = value_item(STR_DIAG_LAST_START, diagnostics_row_start);
    items[4] = value_item(STR_DIAG_LAST_STEP, diagnostics_row_last_step);
    items[5] = value_item(STR_DIAG_LAST_BATTERY, diagnostics_row_last_battery);
    items[6] = value_item(STR_DIAG_LAST_USB, diagnostics_row_usb_last);
    items[7] = value_item(STR_DIAG_USB_ERRORS, diagnostics_row_usb_errors);
    items[8] = value_item(STR_DIAG_JACK_SINGLE_ENDED, diagnostics_row_jack_single_ended);
    items[9] = value_item(STR_DIAG_JACK_BALANCED, diagnostics_row_jack_balanced);
    items[10] = value_item(STR_DIAG_PANEL, diagnostics_row_panel);
    items[11] = action_item(STR_DIAG_CLOCK, diagnostics_row_clock_read, diagnostics_row_clock);
    items[12] = value_item(STR_DIAG_LAST_UPDATE, diagnostics_row_last_update);
    items[13] = value_item(STR_DIAG_LAST_STACK, diagnostics_row_last_stack);
    items[14] = value_item(STR_DIAG_STACK, diagnostics_row_stack);
    items[15] = value_item(STR_DIAG_POWER, diagnostics_row_power);
    items[16] =
        action_item(STR_DIAG_SAVINGS, diagnostics_row_savings_next, diagnostics_row_savings);
    app_screens_build_menu(&screens->diagnostics, i18n_text(STR_DIAG_TITLE), items,
                           RELEASE_DIAGNOSTICS_COUNT, theme);
}

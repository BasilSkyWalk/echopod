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
    items[0] = app_screens_open_item("按键测试", test_pattern_screen_init(&screens->test_pattern));
    items[1] = value_item("固件", diagnostics_row_firmware);
    items[2] = value_item("电池", diagnostics_row_battery);
    items[3] = value_item("上次启动", diagnostics_row_start);
    items[4] = value_item("上次步骤", diagnostics_row_last_step);
    items[5] = value_item("上次电量", diagnostics_row_last_battery);
    items[6] = value_item("上次 USB", diagnostics_row_usb_last);
    items[7] = value_item("3.5 接口", diagnostics_row_jack_single_ended);
    items[8] = value_item("4.4 接口", diagnostics_row_jack_balanced);
    app_screens_build_menu(&screens->diagnostics, "诊断", items, RELEASE_DIAGNOSTICS_COUNT,
                           theme);
}

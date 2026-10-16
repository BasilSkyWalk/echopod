#include "bluetooth_screen.h"

#include "core/string_builder.h"
#include "ui/header.h"
#include "ui/label.h"
#include "ui/navigation.h"

enum {
    FIRST_LINE_TOP = 28,
    LINE_HEIGHT = 15
};

static const char SEPARATOR[] = " · ";

static const char *state_text(HalBluetoothState state)
{
    switch (state) {
    case HAL_BLUETOOTH_STARTING:
        return "Starting";
    case HAL_BLUETOOTH_NO_REPLY:
        return "No reply";
    case HAL_BLUETOOTH_READY:
        return "Ready";
    case HAL_BLUETOOTH_SCANNING:
        return "Scanning";
    default:
        return "Off";
    }
}

static void write_state(const HalBluetoothStatus *status, char *text)
{
    StringBuilder builder = string_builder_make(text, BLUETOOTH_SCREEN_LINE_CAPACITY);
    string_builder_append(&builder, state_text(status->state));
    if (status->version != '\0') {
        string_builder_append(&builder, SEPARATOR);
        string_builder_append_char(&builder, 'v');
        string_builder_append_char(&builder, status->version);
    }
    if (status->link_status != '\0') {
        string_builder_append(&builder, SEPARATOR);
        string_builder_append(&builder, "status ");
        string_builder_append_char(&builder, status->link_status);
    }
}

static void write_link(const HalBluetoothLink *link, char *text)
{
    StringBuilder builder = string_builder_make(text, BLUETOOTH_SCREEN_LINE_CAPACITY);
    string_builder_append(&builder, "RX ");
    string_builder_append_unsigned(&builder, link->received);
    string_builder_append(&builder, SEPARATOR);
    string_builder_append_unsigned(&builder, link->frames);
    string_builder_append(&builder, " frames");
    string_builder_append(&builder, SEPARATOR);
    string_builder_append(&builder, "OE ");
    string_builder_append_unsigned(&builder, link->overruns);
    string_builder_append(&builder, SEPARATOR);
    string_builder_append(&builder, "FIFO ");
    string_builder_append_unsigned(&builder, link->fifo_peak);
    string_builder_append_char(&builder, '/');
    string_builder_append_unsigned(&builder, link->fifo_depth);
}

static char hex_digit(uint8_t value)
{
    return (char)(value < 10 ? '0' + value : 'A' + value - 10);
}

static void write_recent(const HalBluetoothLink *link, char *text)
{
    StringBuilder builder = string_builder_make(text, BLUETOOTH_SCREEN_LINE_CAPACITY);
    if (link->recent_length == 0) {
        string_builder_append(&builder, "Nothing received");
        return;
    }
    for (uint8_t index = 0; index < link->recent_length; index++) {
        if (index > 0) {
            string_builder_append_char(&builder, ' ');
        }
        string_builder_append_char(&builder, hex_digit(link->recent[index] >> 4));
        string_builder_append_char(&builder, hex_digit(link->recent[index] & 0x0F));
    }
}

static void write_device(const HalBluetoothStatus *status, uint8_t index, char *text)
{
    StringBuilder builder = string_builder_make(text, BLUETOOTH_SCREEN_LINE_CAPACITY);
    if (index >= status->device_count) {
        return;
    }
    string_builder_append_unsigned(&builder, (uint32_t)index + 1);
    string_builder_append(&builder, ". ");
    string_builder_append(&builder, status->devices[index].name);
}

static void refresh(BluetoothScreen *bluetooth, const HalBluetoothStatus *status)
{
    write_state(status, bluetooth->lines[0]);
    write_link(&status->link, bluetooth->lines[1]);
    write_recent(&status->link, bluetooth->lines[2]);
    for (uint8_t index = 0; index < HAL_BLUETOOTH_DEVICE_CAPACITY; index++) {
        write_device(status, index, bluetooth->lines[BLUETOOTH_SCREEN_STATUS_LINES + index]);
    }
}

static void enter(void *context, uint32_t now_ms)
{
    BluetoothScreen *bluetooth = context;
    (void)now_ms;
    bluetooth->scanned = false;
    hal_bluetooth_set_power(true);
    refresh(bluetooth, hal_bluetooth_status());
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    BluetoothScreen *bluetooth = context;
    (void)now_ms;
    ScreenAction exit = navigation_exit_action(event);
    if (exit.type != SCREEN_ACTION_NONE) {
        hal_bluetooth_set_power(false);
        return exit;
    }
    if (navigation_from_key(event).intent == NAVIGATION_SELECT) {
        hal_bluetooth_scan();
        bluetooth->scanned = true;
    }
    return screen_action_none();
}

static void update(void *context, uint32_t now_ms)
{
    BluetoothScreen *bluetooth = context;
    (void)now_ms;
    if (!bluetooth->scanned && hal_bluetooth_status()->state == HAL_BLUETOOTH_READY) {
        hal_bluetooth_scan();
        bluetooth->scanned = true;
    }
    refresh(bluetooth, hal_bluetooth_status());
}

static void draw(void *context, Canvas *canvas)
{
    const BluetoothScreen *bluetooth = context;
    const Theme *theme = bluetooth->theme;
    Rect content = theme_content_area(theme);
    header_draw(canvas, theme, bluetooth->screen.title);
    for (int index = 0; index < BLUETOOTH_SCREEN_LINE_COUNT; index++) {
        bool status_line = index < BLUETOOTH_SCREEN_STATUS_LINES;
        TextStyle style = {.font = theme->fonts.small,
                           .color =
                               status_line ? theme->colors.text_secondary : theme->colors.text};
        Rect area = {content.x, FIRST_LINE_TOP + index * LINE_HEIGHT, content.width, LINE_HEIGHT};
        label_draw(canvas, style, (Label){.area = area, .text = bluetooth->lines[index]});
    }
}

Screen *bluetooth_screen_init(BluetoothScreen *bluetooth, const Theme *theme)
{
    *bluetooth = (BluetoothScreen){.theme = theme};
    bluetooth->screen = (Screen){
        .title = "Bluetooth",
        .stays_on_wake = true,
        .context = bluetooth,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &bluetooth->screen;
}

const char *bluetooth_screen_line(const BluetoothScreen *bluetooth, int index)
{
    return bluetooth->lines[index];
}

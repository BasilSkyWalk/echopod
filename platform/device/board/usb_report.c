#include "usb_report.h"

#include <stddef.h>

#include "app/app.h"
#include "board.h"
#include "core/string_builder.h"

enum {
    MESSAGE_CAPACITY = 48,
    HEX_DIGITS = 8
};

static void append_hex(StringBuilder *builder, uint32_t value)
{
    static const char DIGITS[] = "0123456789ABCDEF";
    for (int index = 0; index < HEX_DIGITS; index++) {
        string_builder_append_char(builder, DIGITS[(value >> (28 - 4 * index)) & 0xFu]);
    }
}

static void append_field(StringBuilder *builder, const char *label, uint32_t value)
{
    string_builder_append(builder, label);
    append_hex(builder, value);
    string_builder_append_char(builder, ' ');
}

static void show(const char *title, const char *label_a, uint32_t value_a, const char *label_b,
                 uint32_t value_b)
{
    char text[MESSAGE_CAPACITY];
    StringBuilder builder = string_builder_make(text, sizeof text);
    append_field(&builder, label_a, value_a);
    append_field(&builder, label_b, value_b);
    app_show_notice(title, text);
    board_wait_for_key();
}

void usb_report_show(const UsbReport *report)
{
    uint32_t step = (uint32_t)report->connect_step | (report->internal_ready ? 0x10u : 0u);
    show("USB 1/4: no computer", "ID ", report->after_power_up.core_id, "OTG ",
         report->after_power_up.otg_control);
    show("USB 2/4", "STEP ", step, "CFG3 ", report->after_power_up.hardware_3);
    show("USB 3/4", "INT ", report->after_wait.interrupts, "CFG4 ",
         report->after_power_up.hardware_4);
    show("USB 4/4", "RXF ", report->after_wait.rx_fifo_size, "TXF ",
         report->after_wait.tx_fifo_size);
}

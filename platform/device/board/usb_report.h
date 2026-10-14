#pragma once

#include <stdint.h>

#include "core/string_builder.h"
#include "usb_session.h"

void usb_report_show(const UsbReport *report);
void usb_report_append_hex(StringBuilder *builder, uint32_t value, int digits);

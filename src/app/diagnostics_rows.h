#pragma once

#include "ui/screen.h"

const char *diagnostics_row_firmware(const void *context);
const char *diagnostics_row_battery(const void *context);
const char *diagnostics_row_jack_single_ended(const void *context);
const char *diagnostics_row_jack_balanced(const void *context);
const char *diagnostics_row_start(const void *context);
const char *diagnostics_row_last_step(const void *context);
const char *diagnostics_row_last_battery(const void *context);
const char *diagnostics_row_usb_last(const void *context);
const char *diagnostics_row_usb_errors(const void *context);
const char *diagnostics_row_panel(const void *context);
ScreenAction diagnostics_row_clock_read(void *context);
const char *diagnostics_row_clock(const void *context);

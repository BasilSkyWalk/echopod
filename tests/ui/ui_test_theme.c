#include "ui_test_theme.h"

#include "tiny_font.h"

const Theme TEST_THEME = {
    .colors =
        {
            .background = 0x0000,
            .text = 0xFFFF,
            .text_secondary = 0x7BEF,
            .text_tertiary = 0x4208,
            .selection_background = 0x001F,
            .selection_top = 0x001F,
            .selection_shine = 0x001F,
            .selection_text = 0xFFE0,
            .header_background = 0x8000,
            .header_top = 0x8000,
            .header_text = 0x07FF,
            .divider = 0x4208,
            .scrollbar_thumb = 0xF81F,
            .danger = 0xF800,
            .success = 0x07E0,
            .chevron = 0xC618,
        },
    .fonts = {.title = &TINY_FONT, .body = &TINY_FONT, .small = &TINY_FONT},
    .metrics =
        {
            .screen_width = TEST_SCREEN_WIDTH,
            .screen_height = TEST_SCREEN_HEIGHT,
            .header_height = TEST_HEADER_HEIGHT,
            .header_inset = TEST_HEADER_INSET,
            .row_height = TEST_ROW_HEIGHT,
            .padding = 1,
            .scrollbar_width = 2,
            .scrollbar_inset = 1,
            .detail_gap = 1,
            .status_icon_size = 2,
        },
    .marquee = {.pause_ms = 1000, .speed_px_per_s = 30, .gap_px = 8},
};

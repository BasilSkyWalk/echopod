#include "default_theme.h"

#include "fonts/fonts.h"
#include "hal/display.h"

const Theme DEFAULT_THEME = {
    .colors =
        {
            .background = COLOR_RGB(16, 17, 20),
            .text = COLOR_RGB(242, 242, 245),
            .text_secondary = COLOR_RGB(142, 142, 150),
            .text_tertiary = COLOR_RGB(98, 98, 106),
            .selection_background = COLOR_RGB(34, 96, 208),
            .selection_top = COLOR_RGB(82, 145, 240),
            .selection_shine = COLOR_RGB(120, 170, 248),
            .selection_text = COLOR_RGB(255, 255, 255),
            .header_background = COLOR_RGB(22, 23, 27),
            .header_top = COLOR_RGB(46, 47, 54),
            .header_text = COLOR_RGB(242, 242, 245),
            .divider = COLOR_RGB(48, 49, 56),
            .scrollbar_thumb = COLOR_RGB(96, 97, 106),
            .danger = COLOR_RGB(235, 77, 61),
            .success = COLOR_RGB(52, 199, 89),
            .chevron = COLOR_RGB(110, 110, 118),
        },
    .fonts =
        {
            .title = &FONT_INTER_SEMIBOLD_16,
            .body = &FONT_INTER_REGULAR_16,
            .small = &FONT_INTER_REGULAR_12,
        },
    .metrics =
        {
            .screen_width = HAL_DISPLAY_WIDTH,
            .screen_height = HAL_DISPLAY_HEIGHT,
            .header_height = 24,
            .header_inset = 68,
            .row_height = 24,
            .padding = 10,
            .scrollbar_width = 3,
            .scrollbar_inset = 2,
            .detail_gap = 8,
            .status_icon_size = 10,
        },
    .marquee = {.pause_ms = 1200, .speed_px_per_s = 40, .gap_px = 32},
};

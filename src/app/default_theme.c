#include "default_theme.h"

#include "fonts/fonts.h"
#include "hal/display.h"

const Theme DEFAULT_THEME = {
    .colors =
        {
            .background = COLOR_RGB(16, 17, 20),
            .text = COLOR_RGB(242, 242, 245),
            .text_secondary = COLOR_RGB(142, 142, 150),
            .selection_background = COLOR_RGB(45, 108, 223),
            .selection_text = COLOR_RGB(255, 255, 255),
            .header_background = COLOR_RGB(28, 29, 34),
            .header_text = COLOR_RGB(242, 242, 245),
            .divider = COLOR_RGB(48, 49, 56),
            .scrollbar_track = COLOR_RGB(28, 29, 34),
            .scrollbar_thumb = COLOR_RGB(96, 97, 106),
            .danger = COLOR_RGB(235, 77, 61),
            .success = COLOR_RGB(52, 199, 89),
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
            .detail_gap = 8,
            .status_icon_size = 10,
        },
    .marquee = {.pause_ms = 1200, .speed_px_per_s = 40, .gap_px = 32},
};

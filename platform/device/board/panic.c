#include <stdbool.h>
#include <stdint.h>

#include "board.h"
#include "core/string_builder.h"
#include "device/panic_text.h"
#include "hal/display.h"
#include "hal/system.h"
#include "usb.h"

enum {
    SCALE = 2,
    GLYPH_WIDTH = PANIC_GLYPH_ADVANCE * SCALE,
    GLYPH_HEIGHT = PANIC_GLYPH_ROWS * SCALE,
    LEFT = 8,
    TOP = 8,
    LINE_HEIGHT = GLYPH_HEIGHT + 6,
    LINE_CAPACITY = 32,
    HEX_DIGITS = 8,
    MILLISECONDS_PER_SECOND = 1000,
    POWER_HOLD_MS = 2000,
    STACKED_LR_WORD = 5,
    STACKED_PC_WORD = 6,
    FRAME_WORDS = 8,
    WORDS_PER_LINE = 3,
    WHITE = 0xFFFF,
    BLACK = 0x0000
};

static uint16_t glyph[GLYPH_WIDTH * GLYPH_HEIGHT];
static int line;
static bool power_down;
static uint32_t power_down_since_ms;

static void draw_line(const char *text)
{
    int x = LEFT;
    int y = TOP + line * LINE_HEIGHT;
    for (; *text != '\0' && x + GLYPH_WIDTH <= HAL_DISPLAY_WIDTH; text++, x += GLYPH_WIDTH) {
        panic_render_glyph(*text, SCALE, (PanicColors){WHITE, BLACK}, glyph);
        hal_display_write(x, y, GLYPH_WIDTH, GLYPH_HEIGHT, glyph);
    }
    line++;
}

static void append_hex(StringBuilder *builder, uint32_t value)
{
    static const char DIGITS[] = "0123456789ABCDEF";
    for (int digit = HEX_DIGITS - 1; digit >= 0; digit--) {
        string_builder_append_char(builder, DIGITS[(value >> (4 * digit)) & 0xFu]);
    }
}

static void draw_hex_line(const char *label, uint32_t value)
{
    char text[LINE_CAPACITY];
    StringBuilder builder = string_builder_make(text, sizeof text);
    string_builder_append(&builder, label);
    append_hex(&builder, value);
    draw_line(text);
}

static void draw_run_line(void)
{
    BoardRunSnapshot run = board_start_record_snapshot();
    char text[LINE_CAPACITY];
    StringBuilder builder = string_builder_make(text, sizeof text);
    string_builder_append(&builder, "UP ");
    string_builder_append_unsigned(&builder, run.uptime_ms / MILLISECONDS_PER_SECOND);
    string_builder_append(&builder, " STEP ");
    string_builder_append_unsigned(&builder, run.step);
    string_builder_append(&builder, run.step_finished ? " MV " : " U MV ");
    string_builder_append_unsigned(&builder, run.millivolts);
    draw_line(text);
}

static void start_screen(const char *title)
{
    usb_silence_audio();
    hal_display_set_rotation(HAL_DISPLAY_LANDSCAPE);
    board_display_clear();
    board_backlight_full();
    line = 0;
    draw_line(title);
}

void board_panic_stall(uint32_t pc, uint32_t lr)
{
    start_screen("STALL");
    draw_hex_line("PC ", pc);
    draw_hex_line("LR ", lr);
    draw_run_line();
}

static void draw_pair_line(const char *first_label, uint32_t first, const char *second_label,
                           uint32_t second)
{
    char text[LINE_CAPACITY];
    StringBuilder builder = string_builder_make(text, sizeof text);
    string_builder_append(&builder, first_label);
    append_hex(&builder, first);
    string_builder_append_char(&builder, ' ');
    string_builder_append(&builder, second_label);
    append_hex(&builder, second);
    draw_line(text);
}

static void draw_words_line(const uint32_t *words)
{
    char text[LINE_CAPACITY];
    StringBuilder builder = string_builder_make(text, sizeof text);
    for (int index = 0; index < WORDS_PER_LINE; index++) {
        if (index > 0) {
            string_builder_append_char(&builder, ' ');
        }
        append_hex(&builder, words[index]);
    }
    draw_line(text);
}

void board_panic_fault(const uint32_t *frame, uint32_t status)
{
    start_screen("FAULT");
    draw_pair_line("PC ", frame[STACKED_PC_WORD], "LR ", frame[STACKED_LR_WORD]);
    draw_pair_line("CFSR ", status, "SP ", (uint32_t)(uintptr_t)frame);
    draw_words_line(frame + FRAME_WORDS);
    draw_words_line(frame + FRAME_WORDS + WORDS_PER_LINE);
    draw_run_line();
}

void board_panic_poll_power(uint32_t now_ms)
{
    if (!board_power_key_down()) {
        power_down = false;
        return;
    }
    if (!power_down) {
        power_down = true;
        power_down_since_ms = now_ms;
    }
    if (now_ms - power_down_since_ms >= POWER_HOLD_MS) {
        board_power_cut();
    }
}

void hal_system_stall_test(void)
{
    for (;;) {
        __asm__ volatile("nop");
    }
}

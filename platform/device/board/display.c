#include <stddef.h>

#include "board.h"
#include "device/panel.h"
#include "gpio.h"
#include "hal/display.h"
#include "registers.h"

#define PANEL_DIRECTION_BITS 0x03FF9000u
#define PANEL_READ_STROBE (1u << 12)

enum {
    PANEL_READ_STROBE_PIN = 12,
    PANEL_CONTROL_PIN = 15,
    PANEL_FIRST_BUS_PIN = 16,
    PANEL_LAST_BUS_PIN = 25,
    RESET_HIGH_MS = 50,
    RESET_LOW_MS = 150,
    RESET_SETTLE_MS = 30,
    RESET_READY_MS = 120,
    CLEAR_ROWS = 10
};

static const GpioPin BACKLIGHT = {2, 4};
static const GpioPin PANEL_RESET = {2, 15};

static uint32_t port_shadow;

static void write_port(uint32_t word)
{
    volatile uint32_t *port = register_at(GPIO0_BASE + GPIO_DATA);
    *port = word;
    *port = word;
    *port = word | PANEL_BUS_WR;
}

static void send_byte(uint8_t value, bool data)
{
    write_port(panel_bus_word(port_shadow, value, data));
}

static void bus_command(void *context, uint8_t command)
{
    (void)context;
    send_byte(command, false);
}

static void bus_data(void *context, uint8_t value)
{
    (void)context;
    send_byte(value, true);
}

static void bus_pixels(void *context, const uint16_t *pixels, uint32_t count)
{
    (void)context;
    volatile uint32_t *port = register_at(GPIO0_BASE + GPIO_DATA);
    uint32_t base = panel_bus_word(port_shadow, 0, true);
    for (uint32_t index = 0; index < count; index++) {
        uint32_t pixel = pixels[index];
        uint32_t high = base | (pixel >> 8) << PANEL_BUS_DATA_SHIFT;
        uint32_t low = base | (pixel & 0xFFu) << PANEL_BUS_DATA_SHIFT;
        *port = high;
        *port = high;
        *port = high | PANEL_BUS_WR;
        *port = low;
        *port = low;
        *port = low | PANEL_BUS_WR;
    }
}

static void bus_delay(void *context, uint32_t milliseconds)
{
    (void)context;
    board_delay_ms(milliseconds);
}

static const PanelBus BUS = {bus_command, bus_data, bus_pixels, bus_delay, NULL};

static void mux_panel_pins(void)
{
    for (uint8_t pin = PANEL_FIRST_BUS_PIN; pin <= PANEL_LAST_BUS_PIN; pin++) {
        gpio_mux((GpioPin){0, pin}, GPIO_FUNCTION_GPIO);
    }
    gpio_mux((GpioPin){0, PANEL_CONTROL_PIN}, GPIO_FUNCTION_GPIO);
    gpio_mux((GpioPin){0, PANEL_READ_STROBE_PIN}, GPIO_FUNCTION_GPIO);
    gpio_mux(PANEL_RESET, GPIO_FUNCTION_GPIO);
}

static void set_bus_idle(void)
{
    uint32_t direction = register_read(GPIO0_BASE + GPIO_DIRECTION);
    uint32_t data = register_read(GPIO0_BASE + GPIO_DATA);
    port_shadow = (data & ~PANEL_DIRECTION_BITS) | PANEL_READ_STROBE;
    register_write(GPIO0_BASE + GPIO_DATA, port_shadow | PANEL_BUS_WR);
    register_write(GPIO0_BASE + GPIO_DIRECTION, direction | PANEL_DIRECTION_BITS);
}

static void pulse_reset(uint32_t settle_ms)
{
    gpio_output(PANEL_RESET, true);
    board_delay_ms(RESET_HIGH_MS);
    gpio_output(PANEL_RESET, false);
    board_delay_ms(RESET_LOW_MS);
    gpio_output(PANEL_RESET, true);
    board_delay_ms(settle_ms);
}

static void clear_screen(void)
{
    static uint16_t black[HAL_DISPLAY_WIDTH * CLEAR_ROWS];
    for (int y = 0; y < HAL_DISPLAY_HEIGHT; y += CLEAR_ROWS) {
        hal_display_write(0, y, HAL_DISPLAY_WIDTH, CLEAR_ROWS, black);
    }
}

void board_display_start(void)
{
    gpio_mux(BACKLIGHT, GPIO_FUNCTION_GPIO);
    gpio_output(BACKLIGHT, true);
    mux_panel_pins();
    gpio_output(PANEL_RESET, true);
    set_bus_idle();
    pulse_reset(RESET_SETTLE_MS);
    pulse_reset(RESET_READY_MS);
    panel_send_init(&BUS, PANEL_VARIANT_B);
    clear_screen();
}

void hal_display_write(int x, int y, int width, int height, const uint16_t *pixels)
{
    board_input_poll();
    panel_begin_write(&BUS, (PanelWindow){x, y, width, height});
    bus_pixels(NULL, pixels, (uint32_t)(width * height));
    board_input_poll();
}

void hal_display_present(void)
{
}

void hal_display_set_power(bool on)
{
    gpio_output(BACKLIGHT, on);
}

void hal_display_set_brightness(uint8_t level)
{
    (void)level;
}

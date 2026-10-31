#include <stddef.h>

#include "board.h"
#include "cru.h"
#include "device/backlight.h"
#include "device/panel.h"
#include "gpio.h"
#include "hal/clock.h"
#include "hal/display.h"
#include "registers.h"

#define PANEL_DIRECTION_BITS 0x03FF9000u
#define PANEL_READ_STROBE (1u << 12)
#define PWM_COUNTER (PWM_BASE + 0x0u)
#define PWM_PERIOD (PWM_BASE + 0x4u)
#define PWM_DUTY (PWM_BASE + 0x8u)
#define PWM_CONTROL (PWM_BASE + 0xCu)
#define PWM_CONTROL_RUN 0x00010003u

enum {
    PANEL_READ_STROBE_PIN = 12,
    PANEL_CONTROL_PIN = 15,
    PANEL_FIRST_BUS_PIN = 16,
    PANEL_LAST_BUS_PIN = 25,
    RESET_HIGH_MS = 50,
    RESET_LOW_MS = 150,
    RESET_SETTLE_MS = 30,
    RESET_READY_MS = 120,
    CLEAR_ROWS = 10,
    PWM_GATE = 0x90,
    PWM_RESET = 0x17,
    PWM_RESET_MS = 1
};

static const GpioPin BACKLIGHT = {2, 4};
static const GpioPin PANEL_RESET = {2, 15};

static uint32_t port_shadow;
static PanelRotation rotation;
static uint8_t brightness;
static bool backlight_on;
static bool pwm_running;
static uint32_t pwm_period;
static bool ramping;
static uint32_t ramp_started_ms;

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

void board_display_clear(void)
{
    static uint16_t black[HAL_DISPLAY_WIDTH * CLEAR_ROWS];
    for (int y = 0; y < HAL_DISPLAY_HEIGHT; y += CLEAR_ROWS) {
        hal_display_write(0, y, HAL_DISPLAY_WIDTH, CLEAR_ROWS, black);
    }
}

static void stop_pwm(void)
{
    if (!pwm_running) {
        return;
    }
    register_write(PWM_CONTROL, 0);
    cru_gate(PWM_GATE, false);
    pwm_running = false;
}

static void drive_backlight(bool level)
{
    stop_pwm();
    gpio_mux(BACKLIGHT, GPIO_FUNCTION_GPIO);
    gpio_output(BACKLIGHT, level);
}

static void start_pwm(void)
{
    cru_gate(PWM_GATE, true);
    cru_reset(PWM_RESET, true);
    board_delay_ms(PWM_RESET_MS);
    cru_reset(PWM_RESET, false);
    pwm_running = true;
}

static void run_pwm(BacklightPwm pwm)
{
    if (!pwm_running) {
        start_pwm();
    }
    register_write(PWM_CONTROL, 0);
    register_write(PWM_DUTY, pwm.duty);
    register_write(PWM_COUNTER, 0);
    register_write(PWM_PERIOD, pwm.period);
    register_write(PWM_CONTROL, PWM_CONTROL_RUN);
    pwm_period = pwm.period;
    gpio_output(BACKLIGHT, true);
    gpio_mux(BACKLIGHT, GPIO_FUNCTION_ALTERNATE);
}

static void apply_backlight(void)
{
    if (!backlight_on) {
        drive_backlight(false);
        return;
    }
    BacklightPwm pwm = backlight_pwm(board_pclk_hz(), backlight_percent(brightness));
    if (pwm.full) {
        drive_backlight(true);
    } else {
        run_pwm(pwm);
    }
}

bool board_display_lit(void)
{
    return backlight_on;
}

void board_backlight_full(void)
{
    ramping = false;
    backlight_on = true;
    drive_backlight(true);
}

static void start_fade_in(void)
{
    ramping = true;
    ramp_started_ms = hal_clock_milliseconds();
    run_pwm(backlight_pwm(board_pclk_hz(), backlight_ramp_percent(0, 0)));
}

void board_display_service(void)
{
    if (!ramping) {
        return;
    }
    uint32_t elapsed_ms = hal_clock_milliseconds() - ramp_started_ms;
    if (elapsed_ms >= BACKLIGHT_RAMP_MS || !pwm_running) {
        ramping = false;
        apply_backlight();
        return;
    }
    uint32_t percent = backlight_ramp_percent(backlight_percent(brightness), elapsed_ms);
    register_write(PWM_DUTY, backlight_duty(pwm_period, percent));
}

void board_backlight_off(void)
{
    ramping = false;
    backlight_on = false;
    drive_backlight(false);
}

void board_display_after_clock_change(void)
{
    if (pwm_running) {
        apply_backlight();
    }
}

void board_display_start(void)
{
    brightness = HAL_DISPLAY_BRIGHTNESS_MAX;
    backlight_on = false;
    drive_backlight(false);
    mux_panel_pins();
    gpio_output(PANEL_RESET, true);
    set_bus_idle();
    pulse_reset(RESET_SETTLE_MS);
    pulse_reset(RESET_READY_MS);
    panel_send_init(&BUS, PANEL_VARIANT_B);
    board_display_clear();
}

void hal_display_set_rotation(HalDisplayRotation turned)
{
    rotation = (PanelRotation)turned;
    panel_set_rotation(&BUS, rotation);
}

void hal_display_write(int x, int y, int width, int height, const uint16_t *pixels)
{
    panel_begin_write(&BUS, (PanelWindow){x, y, width, height, rotation});
    bus_pixels(NULL, pixels, (uint32_t)(width * height));
}

void hal_display_present(void)
{
}

void hal_display_set_power(bool on)
{
    board_start_record_step(on ? HAL_START_STEP_BACKLIGHT_ON : HAL_START_STEP_BACKLIGHT_OFF);
    bool was_on = backlight_on;
    backlight_on = on;
    ramping = false;
    if (on && !was_on) {
        start_fade_in();
    } else {
        apply_backlight();
    }
    board_start_record_step_done();
}

void hal_display_set_brightness(uint8_t level)
{
    brightness = level;
    if (backlight_on && !ramping) {
        apply_backlight();
    }
}

#include "hal/input.h"
#include "board.h"
#include "device/key_ladder.h"
#include "device/key_scanner.h"
#include "gpio.h"
#include "hal/clock.h"
#include "registers.h"

#define ADC_RESULT (SARADC_BASE + 0x00u)
#define ADC_CONTROL (SARADC_BASE + 0x08u)
#define ADC_RESULT_MASK 0x3FFu
#define ADC_CHANNEL_MASK 0x7u
#define ADC_KEY_CHANNEL 0x1u
#define ADC_POWER_ON (1u << 3)
#define ADC_START (1u << 4)
#define ADC_INTERRUPT_STATUS (1u << 6)

enum {
    SCAN_PERIOD_MS = 5
};

static const GpioPin POWER_KEY = {2, 5};

static KeyScanner scanner;
static uint32_t last_scan_ms;
static bool conversion_started;

static void start_conversion(void)
{
    uint32_t control = register_read(ADC_CONTROL) & ~ADC_START;
    register_write(ADC_CONTROL, control);
    control &= ~(ADC_CHANNEL_MASK | ADC_INTERRUPT_STATUS);
    register_write(ADC_CONTROL, control | ADC_KEY_CHANNEL | ADC_POWER_ON | ADC_START);
}

static uint8_t pressed_keys(uint16_t reading)
{
    uint8_t mask = key_ladder_mask(reading);
    return gpio_read(POWER_KEY) ? (uint8_t)(mask | (1u << KEY_POWER)) : mask;
}

void board_input_start(void)
{
    gpio_mux(POWER_KEY, GPIO_FUNCTION_GPIO);
    gpio_input(POWER_KEY);
    key_scanner_init(&scanner);
    start_conversion();
    conversion_started = true;
    last_scan_ms = hal_clock_milliseconds();
}

void board_input_poll(void)
{
    uint32_t now_ms = hal_clock_milliseconds();
    if (!conversion_started || now_ms - last_scan_ms < SCAN_PERIOD_MS) {
        return;
    }
    uint16_t reading = (uint16_t)(register_read(ADC_RESULT) & ADC_RESULT_MASK);
    start_conversion();
    last_scan_ms = now_ms;
    key_scanner_sample(&scanner, pressed_keys(reading), now_ms);
}

bool hal_input_next_transition(HalKeyTransition *transition)
{
    board_input_poll();
    return key_scanner_next(&scanner, transition);
}

void board_wait_for_key(void)
{
    HalKeyTransition transition;
    while (hal_input_next_transition(&transition)) {
    }
    for (;;) {
        if (hal_input_next_transition(&transition) && transition.pressed) {
            return;
        }
    }
}

#include "hal/input.h"
#include "board.h"
#include "device/battery_gauge.h"
#include "device/key_ladder.h"
#include "device/key_scanner.h"
#include "gpio.h"
#include "irq.h"
#include "registers.h"

#define ADC_RESULT (SARADC_BASE + 0x00u)
#define ADC_CONTROL (SARADC_BASE + 0x08u)
#define ADC_RESULT_MASK 0x3FFu
#define ADC_CHANNEL_MASK 0x7u
#define ADC_KEY_CHANNEL 0x1u
#define ADC_BATTERY_CHANNEL 0x0u
#define ADC_POWER_ON (1u << 3)
#define ADC_START (1u << 4)
#define ADC_INTERRUPT_STATUS (1u << 6)

static const GpioPin POWER_KEY = {2, 5};

enum {
    SCANS_PER_BATTERY_READING = 20
};

static KeyScanner scanner;
static BatteryGauge gauge;
static bool conversion_started;
static bool battery_pending;
static uint32_t scans;

static void start_conversion(uint32_t channel)
{
    uint32_t control = register_read(ADC_CONTROL) & ~ADC_START;
    register_write(ADC_CONTROL, control);
    control &= ~(ADC_CHANNEL_MASK | ADC_INTERRUPT_STATUS);
    register_write(ADC_CONTROL, control | channel | ADC_POWER_ON | ADC_START);
}

static uint8_t pressed_keys(uint16_t reading)
{
    uint8_t mask = key_ladder_mask(reading);
    return gpio_read(POWER_KEY) ? (uint8_t)(mask | (1u << KEY_POWER)) : mask;
}

static void record_battery(void)
{
    if (battery_gauge_known(&gauge)) {
        board_start_record_battery(battery_gauge_millivolts(&gauge));
    }
}

void board_input_start(void)
{
    gpio_mux(POWER_KEY, GPIO_FUNCTION_GPIO);
    gpio_input(POWER_KEY);
    key_scanner_init(&scanner);
    battery_gauge_init(&gauge);
    start_conversion(ADC_KEY_CHANNEL);
    conversion_started = true;
}

void board_input_sample(uint32_t now_ms)
{
    if (!conversion_started) {
        return;
    }
    uint16_t reading = (uint16_t)(register_read(ADC_RESULT) & ADC_RESULT_MASK);
    if (battery_pending) {
        battery_gauge_add(&gauge, reading);
        battery_pending = false;
        record_battery();
    } else {
        key_scanner_sample(&scanner, pressed_keys(reading), now_ms);
    }
    scans++;
    battery_pending = scans % SCANS_PER_BATTERY_READING == 0;
    start_conversion(battery_pending ? ADC_BATTERY_CHANNEL : ADC_KEY_CHANNEL);
}

BatteryGauge board_battery_gauge(void)
{
    uint32_t primask = irq_save();
    BatteryGauge copy = gauge;
    irq_restore(primask);
    return copy;
}

bool hal_input_next_transition(HalKeyTransition *transition)
{
    uint32_t primask = irq_save();
    bool found = key_scanner_next(&scanner, transition);
    irq_restore(primask);
    return found;
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

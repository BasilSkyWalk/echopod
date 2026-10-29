#include "board.h"
#include "device/jack_sense.h"
#include "gpio.h"
#include "hal/audio.h"

static const GpioPin SINGLE_ENDED_SENSE = {0, 13};
static const GpioPin BALANCED_SENSE = {2, 8};

static bool pins_ready;
static HalJackPins boot_state;
static JackSense sense;

static HalJackPin boot_pin(GpioPin pin)
{
    return (HalJackPin){.high = false,
                        .boot_function = (uint8_t)gpio_function(pin),
                        .boot_output = gpio_is_output(pin),
                        .pull = gpio_pull_enabled(pin)};
}

static void prepare_pins(void)
{
    if (pins_ready) {
        return;
    }
    boot_state = (HalJackPins){.single_ended = boot_pin(SINGLE_ENDED_SENSE),
                               .balanced = boot_pin(BALANCED_SENSE)};
    gpio_mux(SINGLE_ENDED_SENSE, GPIO_FUNCTION_GPIO);
    gpio_input(SINGLE_ENDED_SENSE);
    gpio_mux(BALANCED_SENSE, GPIO_FUNCTION_GPIO);
    gpio_input(BALANCED_SENSE);
    pins_ready = true;
}

HalJackPins hal_audio_jack_pins(void)
{
    prepare_pins();
    HalJackPins pins = boot_state;
    pins.single_ended.high = gpio_read(SINGLE_ENDED_SENSE);
    pins.balanced.high = gpio_read(BALANCED_SENSE);
    return pins;
}

bool board_jack_poll(void)
{
    prepare_pins();
    if (board_bluetooth_routed() || board_bluetooth_settling()) {
        return false;
    }
    return jack_sense_sample(
        &sense, jack_kind_of(gpio_read(SINGLE_ENDED_SENSE), gpio_read(BALANCED_SENSE)));
}

HalAudioJack hal_audio_jack(void)
{
    if (!sense.started) {
        board_jack_poll();
    }
    switch (sense.stable) {
    case JACK_SINGLE_ENDED:
        return HAL_AUDIO_JACK_SINGLE_ENDED;
    case JACK_BALANCED:
        return HAL_AUDIO_JACK_BALANCED;
    case JACK_NONE:
        break;
    }
    return HAL_AUDIO_JACK_NONE;
}

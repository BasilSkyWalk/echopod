#include "gpio.h"

#include "registers.h"

enum {
    GRF_MUX_PORT_STRIDE = 0x20,
    GRF_MUX_PINS_PER_REGISTER = 8,
    GRF_WRITE_ENABLE_SHIFT = 16
};

uint32_t gpio_port_base(uint8_t port)
{
    static const uint32_t BASES[] = {GPIO0_BASE, GPIO1_BASE, GPIO2_BASE};
    return BASES[port];
}

void gpio_mux(GpioPin pin, uint32_t function)
{
    uint32_t address = GRF_BASE + GRF_MUX_PORT_STRIDE * pin.port +
                       4u * (uint32_t)(pin.pin / GRF_MUX_PINS_PER_REGISTER);
    uint32_t shift = 2u * (uint32_t)(pin.pin % GRF_MUX_PINS_PER_REGISTER);
    register_write(address, (function | (3u << GRF_WRITE_ENABLE_SHIFT)) << shift);
}

static void set_bit(uint32_t address, uint8_t bit, bool value)
{
    uint32_t mask = 1u << bit;
    uint32_t current = register_read(address);
    register_write(address, value ? current | mask : current & ~mask);
}

void gpio_output(GpioPin pin, bool level)
{
    uint32_t base = gpio_port_base(pin.port);
    set_bit(base + GPIO_DATA, pin.pin, level);
    set_bit(base + GPIO_DIRECTION, pin.pin, true);
}

void gpio_input(GpioPin pin)
{
    set_bit(gpio_port_base(pin.port) + GPIO_DIRECTION, pin.pin, false);
}

bool gpio_read(GpioPin pin)
{
    return (register_read(gpio_port_base(pin.port) + GPIO_INPUT_LEVEL) >> pin.pin & 1u) != 0;
}

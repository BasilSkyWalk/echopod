#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    GPIO_FUNCTION_GPIO = 0,
    GPIO_FUNCTION_ALTERNATE = 1
};

typedef struct GpioPin {
    uint8_t port;
    uint8_t pin;
} GpioPin;

uint32_t gpio_port_base(uint8_t port);
void gpio_mux(GpioPin pin, uint32_t function);
void gpio_output(GpioPin pin, bool level);
void gpio_input(GpioPin pin);
bool gpio_read(GpioPin pin);

#include <stdbool.h>
#include <stdint.h>

#include "board.h"
#include "cru.h"
#include "device/pcf8563.h"
#include "gpio.h"
#include "hal/time.h"
#include "i2c.h"
#include "registers.h"

#define I2C1_BASE 0x40050000u
#define GRF_PIN_GROUPS (GRF_BASE + 0xBCu)
#define GRF_I2C1_GROUP_C 0x000C0008u

enum {
    GATE_I2C1 = 0x84,
    RESET_I2C1 = 0x1B,
    I2C1_PIN_FUNCTION = 3,
    RTC_BUS_KHZ = 100,
    RTC_TIMEOUT_MS = 2,
    RESET_SETTLE_US = 200
};

static const GpioPin RTC_PINS[] = {{2, 12}, {2, 13}};
static const uint8_t READ_HEADER[] = {PCF8563_WRITE_ADDRESS, PCF8563_FIRST_REGISTER};

static void start_bus(I2cPort *port)
{
    cru_gate(GATE_I2C1, true);
    gpio_mux(RTC_PINS[0], I2C1_PIN_FUNCTION);
    gpio_mux(RTC_PINS[1], I2C1_PIN_FUNCTION);
    register_write(GRF_PIN_GROUPS, GRF_I2C1_GROUP_C);
    cru_reset(RESET_I2C1, true);
    board_delay_us(RESET_SETTLE_US);
    cru_reset(RESET_I2C1, false);
    i2c_port_start(port, I2C1_BASE, RTC_TIMEOUT_MS, RTC_BUS_KHZ);
}

bool hal_time_read(HalTime *time)
{
    I2cPort port;
    uint8_t registers[PCF8563_REGISTER_COUNT];
    *time = (HalTime){.present = false};
    start_bus(&port);
    if (!i2c_receive(&port, READ_HEADER, sizeof READ_HEADER, registers, sizeof registers)) {
        return false;
    }
    pcf8563_decode(registers, time);
    return true;
}

bool hal_time_write(const HalTime *time)
{
    I2cPort port;
    uint8_t run[] = {PCF8563_WRITE_ADDRESS, PCF8563_CONTROL_REGISTER, 0};
    uint8_t frame[2 + PCF8563_TIME_REGISTER_COUNT] = {PCF8563_WRITE_ADDRESS, PCF8563_TIME_REGISTER};
    pcf8563_encode(time, frame + 2);
    start_bus(&port);
    return i2c_send(&port, run, sizeof run) && i2c_send(&port, frame, sizeof frame);
}

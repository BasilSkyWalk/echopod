#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum I2cStage {
    I2C_STAGE_NONE,
    I2C_STAGE_START,
    I2C_STAGE_SEND,
    I2C_STAGE_NAK,
    I2C_STAGE_STOP,
    I2C_STAGE_RECEIVE
} I2cStage;

typedef struct I2cPort {
    uint32_t base;
    uint32_t timeout_ms;
    uint8_t stage;
    uint8_t pending;
} I2cPort;

void i2c_port_start(I2cPort *port, uint32_t base, uint32_t timeout_ms, uint32_t bus_khz);
bool i2c_send(I2cPort *port, const uint8_t *bytes, uint32_t count);
bool i2c_receive(I2cPort *port, const uint8_t *header, uint32_t header_count, uint8_t *bytes,
                 uint32_t count);

#include "i2c.h"

#include "board.h"
#include "device/audio_math.h"
#include "hal/clock.h"
#include "registers.h"

#define I2C_CON 0x00u
#define I2C_CLKDIV 0x04u
#define I2C_MRXADDR 0x08u
#define I2C_MRXRADDR 0x0Cu
#define I2C_MTXCNT 0x10u
#define I2C_MRXCNT 0x14u
#define I2C_IEN 0x18u
#define I2C_IPD 0x1Cu
#define I2C_TXDATA 0x100u
#define I2C_RXDATA 0x200u
#define I2C_MRXADDR_VALID 0x01000000u

enum {
    I2C_ENABLE = 0x01,
    I2C_MODE_RECEIVE = 0x02,
    I2C_START = 0x08,
    I2C_STOP = 0x10,
    I2C_NAK_LAST = 0x20,
    I2C_ALL_PENDING = 0x7F,
    I2C_TX_DONE = 0x04,
    I2C_RX_DONE = 0x08,
    I2C_START_DONE = 0x10,
    I2C_STOP_DONE = 0x20,
    I2C_NAK = 0x40,
    BYTES_PER_WORD = 4,
    BITS_PER_BYTE = 8
};

static uint32_t read_port(const I2cPort *port, uint32_t offset)
{
    return register_read(port->base + offset);
}

static void write_port(const I2cPort *port, uint32_t offset, uint32_t value)
{
    register_write(port->base + offset, value);
}

static void note_failure(I2cPort *port, I2cStage stage)
{
    if (port->stage == I2C_STAGE_NONE) {
        port->stage = (uint8_t)stage;
        port->pending = (uint8_t)read_port(port, I2C_IPD);
    }
}

static bool wait_pending(I2cPort *port, uint32_t bits, I2cStage stage)
{
    uint32_t start = hal_clock_milliseconds();
    while ((read_port(port, I2C_IPD) & bits) == 0) {
        if (hal_clock_milliseconds() - start >= port->timeout_ms) {
            note_failure(port, stage);
            return false;
        }
    }
    return true;
}

static bool start(I2cPort *port, uint32_t mode)
{
    write_port(port, I2C_IPD, I2C_ALL_PENDING);
    write_port(port, I2C_CON, I2C_ENABLE | mode | I2C_START);
    if (!wait_pending(port, I2C_START_DONE, I2C_STAGE_START)) {
        return false;
    }
    write_port(port, I2C_IPD, I2C_START_DONE);
    write_port(port, I2C_CON, I2C_ENABLE | mode);
    return true;
}

static bool stop(I2cPort *port)
{
    write_port(port, I2C_IPD, I2C_ALL_PENDING);
    write_port(port, I2C_CON, I2C_ENABLE | I2C_STOP);
    bool stopped = wait_pending(port, I2C_STOP_DONE, I2C_STAGE_STOP);
    write_port(port, I2C_IPD, I2C_ALL_PENDING);
    write_port(port, I2C_CON, 0);
    return stopped;
}

void i2c_port_start(I2cPort *port, uint32_t base, uint32_t timeout_ms, uint32_t bus_khz)
{
    *port = (I2cPort){.base = base, .timeout_ms = timeout_ms, .stage = I2C_STAGE_NONE};
    write_port(port, I2C_CON, 0);
    write_port(port, I2C_CLKDIV, i2c_clock_divider(board_pclk_hz(), bus_khz));
}

bool i2c_send(I2cPort *port, const uint8_t *bytes, uint32_t count)
{
    if (!start(port, 0)) {
        stop(port);
        return false;
    }
    for (uint32_t word = 0; word * BYTES_PER_WORD < count; word++) {
        uint32_t value = 0;
        for (uint32_t byte = 0; byte < BYTES_PER_WORD && word * BYTES_PER_WORD + byte < count;
             byte++) {
            value |= (uint32_t)bytes[word * BYTES_PER_WORD + byte] << (BITS_PER_BYTE * byte);
        }
        write_port(port, I2C_TXDATA + BYTES_PER_WORD * word, value);
    }
    write_port(port, I2C_IPD, I2C_ALL_PENDING);
    write_port(port, I2C_MTXCNT, count);
    bool sent = wait_pending(port, I2C_TX_DONE | I2C_NAK, I2C_STAGE_SEND);
    if (sent && (read_port(port, I2C_IPD) & I2C_NAK) != 0) {
        note_failure(port, I2C_STAGE_NAK);
        sent = false;
    }
    return stop(port) && sent;
}

bool i2c_receive(I2cPort *port, const uint8_t *header, uint32_t header_count, uint8_t *bytes,
                 uint32_t count)
{
    if (!i2c_send(port, header, header_count)) {
        return false;
    }
    write_port(port, I2C_IPD, I2C_ALL_PENDING);
    write_port(port, I2C_IEN, 0);
    write_port(port, I2C_MRXADDR, I2C_MRXADDR_VALID + header[0] + 1u);
    write_port(port, I2C_MRXRADDR, 0);
    if (!start(port, I2C_MODE_RECEIVE)) {
        stop(port);
        return false;
    }
    write_port(port, I2C_IPD, I2C_ALL_PENDING);
    write_port(port, I2C_CON, read_port(port, I2C_CON) | I2C_NAK_LAST);
    write_port(port, I2C_MRXCNT, count);
    bool received = wait_pending(port, I2C_RX_DONE, I2C_STAGE_RECEIVE);
    for (uint32_t index = 0; index < count; index++) {
        uint32_t word = read_port(port, I2C_RXDATA + index - index % BYTES_PER_WORD);
        bytes[index] = (uint8_t)(word >> (BITS_PER_BYTE * (index % BYTES_PER_WORD)));
    }
    return stop(port) && received;
}

#include "hal/bluetooth.h"
#include "board.h"
#include "cru.h"
#include "device/bt_module.h"
#include "gpio.h"
#include "hal/clock.h"
#include "registers.h"

#define UART1_BASE 0x400B0000u
#define UART_DATA 0x00u
#define UART_DIVISOR_HIGH 0x04u
#define UART_INTERRUPT_ENABLE 0x04u
#define UART_LINE_CONTROL 0x0Cu
#define UART_MODEM_CONTROL 0x10u
#define UART_LINE_STATUS 0x14u
#define UART_STATUS 0x7Cu
#define UART_RX_LEVEL 0x84u
#define UART_SOFTWARE_RESET 0x88u
#define UART_FIFO_ENABLE 0x98u
#define UART_RX_TRIGGER 0x9Cu
#define UART_TX_EMPTY_TRIGGER 0xA0u
#define UART_DMA_ACK 0xA8u
#define UART_PARAMETERS 0xF4u
#define GRF_IOMUX_CON 0xBCu
#define GRF_UART1_GROUP_A 0x02000000u
#define CRU_UART_CLOCK 0x38u
#define CRU_UART1_CRYSTAL 0x00C00040u
#define CRU_UART1_DIVIDER_ONE 0x003F0000u

enum {
    UART1_RESET = 0x08,
    UART1_GATE = 0x1D,
    UART1_BUS_GATE = 0x89,
    RESET_ALL = 7,
    RESET_FIFOS = 6,
    LINE_8N1 = 3,
    DIVISOR_LATCH = 0x80,
    DIVISOR_115200 = 13,
    LINE_DATA_READY = 0x01,
    LINE_OVERRUN = 0x02,
    STATUS_TX_NOT_FULL = 0x02,
    FIFO_MODE_SHIFT = 16,
    FIFO_MODE_UNIT = 16,
    TX_WAIT_POLLS = 100000,
    RX_BYTES_PER_SERVICE = 256
};

static const GpioPin MODULE_POWER = {2, 7};
static const GpioPin UART_PIN_A = {2, 16};
static const GpioPin UART_PIN_B = {2, 17};
static const GpioPin AUDIO_MUTE = {2, 0};
static const GpioPin WIRED_OUTPUT = {2, 14};

static BtModule module;
static bool powered;
static bool routed;

static void uart_write(uint32_t offset, uint32_t value)
{
    register_write(UART1_BASE + offset, value);
}

static uint32_t uart_read(uint32_t offset)
{
    return register_read(UART1_BASE + offset);
}

static void pins_off(void)
{
    gpio_mux(MODULE_POWER, GPIO_FUNCTION_GPIO);
    gpio_output(MODULE_POWER, false);
    gpio_mux(UART_PIN_B, GPIO_FUNCTION_GPIO);
    gpio_output(UART_PIN_B, false);
    gpio_mux(UART_PIN_A, GPIO_FUNCTION_GPIO);
    gpio_output(UART_PIN_A, false);
}

void board_bluetooth_off(void)
{
    pins_off();
}

static void uart_clock_on(void)
{
    cru_reset(UART1_RESET, true);
    board_delay_ms(1);
    cru_reset(UART1_RESET, false);
    cru_gate(UART1_GATE, true);
    cru_gate(UART1_BUS_GATE, true);
    cru_write(CRU_UART_CLOCK, CRU_UART1_CRYSTAL);
    cru_write(CRU_UART_CLOCK, CRU_UART1_DIVIDER_ONE);
}

static void uart_configure(void)
{
    uart_write(UART_SOFTWARE_RESET, RESET_ALL);
    uart_write(UART_INTERRUPT_ENABLE, 0);
    uart_write(UART_DMA_ACK, 1);
    uart_write(UART_MODEM_CONTROL, 0);
    uart_write(UART_FIFO_ENABLE, 1);
    uart_write(UART_RX_TRIGGER, 0);
    uart_write(UART_TX_EMPTY_TRIGGER, 0);
    uart_write(UART_LINE_CONTROL, LINE_8N1);
    uart_write(UART_LINE_CONTROL, LINE_8N1 | DIVISOR_LATCH);
    uart_write(UART_DATA, DIVISOR_115200);
    uart_write(UART_DIVISOR_HIGH, 0);
    uart_write(UART_LINE_CONTROL, LINE_8N1);
    uart_write(UART_SOFTWARE_RESET, RESET_FIFOS);
}

static void uart_open(void)
{
    register_write(GRF_BASE + GRF_IOMUX_CON, GRF_UART1_GROUP_A);
    uart_clock_on();
    uart_configure();
    gpio_mux(UART_PIN_A, GPIO_FUNCTION_ALTERNATE);
    gpio_mux(UART_PIN_B, GPIO_FUNCTION_ALTERNATE);
}

static uint16_t fifo_depth(void)
{
    return (uint16_t)((uart_read(UART_PARAMETERS) >> FIFO_MODE_SHIFT & 0xFFu) * FIFO_MODE_UNIT);
}

static void power_on(void)
{
    pins_off();
    uart_open();
    gpio_output(MODULE_POWER, true);
    bt_module_power_on(&module, hal_clock_milliseconds());
    module.status.link.fifo_depth = fifo_depth();
    powered = true;
}

static void route_to_module(bool to_module)
{
    gpio_output(AUDIO_MUTE, to_module);
    gpio_output(WIRED_OUTPUT, !to_module);
    routed = to_module;
}

static void follow_stream(void)
{
    if (module.status.streaming) {
        route_to_module(true);
    } else if (routed) {
        route_to_module(false);
    }
}

static void power_off(void)
{
    pins_off();
    powered = false;
    if (routed) {
        route_to_module(false);
    }
}

void hal_bluetooth_set_power(bool on)
{
    if (on) {
        power_on();
        return;
    }
    power_off();
    bt_module_power_off(&module);
}

static void send(const BtModuleFrame *frame)
{
    for (uint8_t index = 0; index < frame->length; index++) {
        uint32_t polls = 0;
        while ((uart_read(UART_STATUS) & STATUS_TX_NOT_FULL) == 0 && polls < TX_WAIT_POLLS) {
            polls++;
        }
        uart_write(UART_DATA, frame->bytes[index]);
    }
}

void hal_bluetooth_scan(void)
{
    BtModuleFrame frame;
    if (powered && bt_module_scan(&module, hal_clock_milliseconds(), &frame)) {
        send(&frame);
    }
}

void hal_bluetooth_connect(uint8_t index)
{
    BtModuleFrame frame;
    if (powered && bt_module_connect(&module, index, hal_clock_milliseconds(), &frame)) {
        send(&frame);
    }
}

void hal_bluetooth_disconnect(void)
{
    BtModuleFrame frame;
    if (powered && bt_module_disconnect(&module, hal_clock_milliseconds(), &frame)) {
        send(&frame);
    }
}

static void note_fifo_level(void)
{
    uint16_t level = (uint16_t)uart_read(UART_RX_LEVEL);
    if (level > module.status.link.fifo_peak) {
        module.status.link.fifo_peak = level;
    }
}

static void receive(uint32_t now_ms)
{
    note_fifo_level();
    for (int count = 0; count < RX_BYTES_PER_SERVICE; count++) {
        uint32_t line = uart_read(UART_LINE_STATUS);
        if ((line & LINE_OVERRUN) != 0) {
            module.status.link.overruns++;
        }
        if ((line & LINE_DATA_READY) == 0) {
            return;
        }
        bt_module_receive(&module, (uint8_t)uart_read(UART_DATA), now_ms);
    }
}

void hal_bluetooth_service(void)
{
    if (!powered) {
        return;
    }
    uint32_t now_ms = hal_clock_milliseconds();
    receive(now_ms);
    BtModuleFrame frame;
    if (bt_module_poll(&module, now_ms, &frame)) {
        send(&frame);
    }
    follow_stream();
    if (module.status.state == HAL_BLUETOOTH_NO_REPLY) {
        power_off();
    }
}

const HalBluetoothStatus *hal_bluetooth_status(void)
{
    return &module.status;
}

void hal_bluetooth_set_stream(bool on)
{
    BtModuleFrame frame;
    if (powered && bt_module_set_stream(&module, on, hal_clock_milliseconds(), &frame)) {
        send(&frame);
    }
}

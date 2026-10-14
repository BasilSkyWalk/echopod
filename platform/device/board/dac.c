#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board.h"
#include "cru.h"
#include "device/audio_math.h"
#include "device/dac_sequence.h"
#include "device/dma_loop.h"
#include "dmac.h"
#include "gpio.h"
#include "hal/audio.h"
#include "hal/clock.h"
#include "registers.h"

#define I2C0_BASE 0x40040000u
#define I2S1_BASE 0x40030000u
#define ACODEC_BASE 0x40090000u

#define I2C_CON (I2C0_BASE + 0x00u)
#define I2C_CLKDIV (I2C0_BASE + 0x04u)
#define I2C_MRXADDR (I2C0_BASE + 0x08u)
#define I2C_MRXRADDR (I2C0_BASE + 0x0Cu)
#define I2C_MTXCNT (I2C0_BASE + 0x10u)
#define I2C_MRXCNT (I2C0_BASE + 0x14u)
#define I2C_IEN (I2C0_BASE + 0x18u)
#define I2C_IPD (I2C0_BASE + 0x1Cu)
#define I2C_TXDATA (I2C0_BASE + 0x100u)
#define I2C_RXDATA (I2C0_BASE + 0x200u)

#define I2S_TXCR (I2S1_BASE + 0x00u)
#define I2S_RXCR (I2S1_BASE + 0x04u)
#define I2S_CKR (I2S1_BASE + 0x08u)
#define I2S_FIFOLR (I2S1_BASE + 0x0Cu)
#define I2S_DMACR (I2S1_BASE + 0x10u)
#define I2S_INTCR (I2S1_BASE + 0x14u)
#define I2S_XFER (I2S1_BASE + 0x1Cu)
#define I2S_CLR (I2S1_BASE + 0x20u)
#define I2S_TXDR (I2S1_BASE + 0x24u)

#define ACODEC_MUX_CONTROL (ACODEC_BASE + 0x200u)
#define ACODEC_M4 (ACODEC_BASE + 0x204u)
#define ACODEC_M5 (ACODEC_BASE + 0x208u)
#define ACODEC_M6 (ACODEC_BASE + 0x20Cu)
#define ACODEC_M7 (ACODEC_BASE + 0x210u)
#define ACODEC_MUX_ENABLE (ACODEC_BASE + 0x214u)

#define GRF_PIN_GROUPS (GRF_BASE + 0xBCu)
#define GRF_I2C0_GROUP_A 0x00030000u
#define GRF_I2S1_GROUP_A 0x20000000u
#define CRU_ACODEC_SELECT 0x00200020u
#define CRU_I2S_SOURCE 0x80008000u
#define CRU_I2S_DIVIDER 0x007F0004u
#define CRU_I2S_OUTPUT 0x03000000u
#define CRU_I2S_CLOCK 0x10000000u
#define I2S_FORMAT_16_BIT_STEREO 0x401Fu
#define I2S_INTERRUPTS 0x01F00102u
#define I2S_INTERRUPTS_FOR_DMA 0x01F00100u
#define I2S_DMA_TX 0x00000110u
#define I2C_MRXADDR_VALID 0x01000000u

enum {
    GATE_GPIO1 = 0x95,
    RESET_I2C0 = 0x1A,
    RESET_I2S1 = 0x04,
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
    I2C_TIMEOUT_MS = 10,
    FIFO_TIMEOUT_MS = 10,
    I2C_BUS_KHZ = 100,
    I2C_AUTO_INCREMENT = 0x01,
    FRAME_CAPACITY = 8,
    FIFO_LEVEL_MASK = 0x3F,
    FIFO_FULL_LEVEL = 0x1E,
    TONE_RATE_HZ = 44100,
    TONE_MCLK_HZ = 11288960,
    TONE_RATE_BYTE = 1,
    TONE_PERIOD_SAMPLES = 100,
    TONE_AMPLITUDE = 3000,
    TONE_SEGMENT_MS = 2000,
    TONE_GAP_MS = 400,
    DMA_TONE_WORDS = 4000,
    I2S_RESET_MS = 1,
    I2S_CLEAR_MS = 2,
    ACODEC_SETTLE_MS = 5
};

static const uint32_t SOC_AUDIO_GATES[] = {0x0E, 0x10, 0x11, 0x12, 0x13, 0x82, 0x83, 0x96};
static const uint8_t ACODEC_44100[4] = {0x06, 0x24, 0x19, 0x03};
static const GpioPin I2C_PINS[] = {{2, 10}, {2, 11}};
static const GpioPin I2S_PINS[] = {{1, 0}, {1, 1}, {1, 2}, {1, 3}};
static const GpioPin DAC_PINS[] = {
    [DAC_PIN_MUTE] = {2, 0},  [DAC_PIN_ENABLE_A] = {2, 14}, [DAC_PIN_ENABLE_B] = {2, 2},
    [DAC_PIN_RESET] = {2, 1}, [DAC_PIN_AMP] = {1, 4},
};

static bool wait_pending(uint32_t bits)
{
    uint32_t start = hal_clock_milliseconds();
    while ((register_read(I2C_IPD) & bits) == 0) {
        if (hal_clock_milliseconds() - start >= I2C_TIMEOUT_MS) {
            return false;
        }
    }
    return true;
}

static bool i2c_start(uint32_t mode)
{
    register_write(I2C_IPD, I2C_ALL_PENDING);
    register_write(I2C_CON, I2C_ENABLE | mode | I2C_START);
    if (!wait_pending(I2C_START_DONE)) {
        return false;
    }
    register_write(I2C_IPD, I2C_START_DONE);
    register_write(I2C_CON, I2C_ENABLE | mode);
    return true;
}

static bool i2c_stop(void)
{
    register_write(I2C_IPD, I2C_ALL_PENDING);
    register_write(I2C_CON, I2C_ENABLE | I2C_STOP);
    bool stopped = wait_pending(I2C_STOP_DONE);
    register_write(I2C_IPD, I2C_ALL_PENDING);
    register_write(I2C_CON, 0);
    return stopped;
}

static bool i2c_send(const uint8_t *bytes, uint32_t count)
{
    if (!i2c_start(0)) {
        i2c_stop();
        return false;
    }
    for (uint32_t word = 0; word * 4 < count; word++) {
        uint32_t value = 0;
        for (uint32_t byte = 0; byte < 4 && word * 4 + byte < count; byte++) {
            value |= (uint32_t)bytes[word * 4 + byte] << (8 * byte);
        }
        register_write(I2C_TXDATA + 4 * word, value);
    }
    register_write(I2C_IPD, I2C_ALL_PENDING);
    register_write(I2C_MTXCNT, count);
    bool sent = wait_pending(I2C_TX_DONE | I2C_NAK) && (register_read(I2C_IPD) & I2C_NAK) == 0;
    return i2c_stop() && sent;
}

static uint32_t frame_header(uint8_t *frame, uint8_t address, uint32_t map)
{
    frame[0] = address;
    frame[1] = (uint8_t)(map >> 16);
    frame[2] = (uint8_t)(map >> 8);
    frame[3] = (uint8_t)map;
    frame[4] = I2C_AUTO_INCREMENT;
    return 5;
}

static bool dac_write(void *context, uint8_t address, uint32_t map, uint8_t value)
{
    (void)context;
    uint8_t frame[FRAME_CAPACITY];
    uint32_t length = frame_header(frame, address, map);
    frame[length++] = value;
    return i2c_send(frame, length);
}

static bool dac_read(void *context, uint8_t address, uint32_t map, uint8_t *value)
{
    (void)context;
    uint8_t frame[FRAME_CAPACITY];
    if (!i2c_send(frame, frame_header(frame, address, map))) {
        return false;
    }
    register_write(I2C_IPD, I2C_ALL_PENDING);
    register_write(I2C_IEN, 0);
    register_write(I2C_MRXADDR, I2C_MRXADDR_VALID + address + 1u);
    register_write(I2C_MRXRADDR, 0);
    if (!i2c_start(I2C_MODE_RECEIVE)) {
        i2c_stop();
        return false;
    }
    register_write(I2C_IPD, I2C_ALL_PENDING);
    register_write(I2C_CON, register_read(I2C_CON) | I2C_NAK_LAST);
    register_write(I2C_MRXCNT, 1);
    bool received = wait_pending(I2C_RX_DONE);
    *value = (uint8_t)register_read(I2C_RXDATA);
    return i2c_stop() && received;
}

static void dac_set_pin(void *context, DacPin pin, bool level)
{
    (void)context;
    gpio_output(DAC_PINS[pin], level);
}

static void dac_delay(void *context, uint32_t microseconds)
{
    (void)context;
    board_delay_us(microseconds);
}

static const DacBus BUS = {dac_write, dac_read, dac_set_pin, dac_delay, NULL};

static void start_soc_clocks(void)
{
    for (uint32_t index = 0; index < sizeof SOC_AUDIO_GATES / sizeof SOC_AUDIO_GATES[0]; index++) {
        cru_gate(SOC_AUDIO_GATES[index], true);
    }
}

static void start_i2c(void)
{
    gpio_mux(I2C_PINS[0], GPIO_FUNCTION_ALTERNATE);
    gpio_mux(I2C_PINS[1], GPIO_FUNCTION_ALTERNATE);
    register_write(GRF_PIN_GROUPS, GRF_I2C0_GROUP_A);
    cru_reset(RESET_I2C0, true);
    board_delay_us(200);
    cru_reset(RESET_I2C0, false);
    register_write(I2C_CON, 0);
    register_write(I2C_CLKDIV, i2c_clock_divider(board_pclk_hz(), I2C_BUS_KHZ));
}

static void set_bits(uint32_t address, uint32_t mask, uint32_t bits)
{
    register_write(address, (register_read(address) & ~mask) | bits);
}

static void start_acodec(const uint8_t *row)
{
    cru_write(0x10, CRU_ACODEC_SELECT);
    set_bits(ACODEC_MUX_CONTROL, 1u << 2, 0);
    set_bits(ACODEC_MUX_ENABLE, 1u << 2, 1u << 2);
    set_bits(ACODEC_MUX_CONTROL, 3u << 3, 2u << 3);
    register_write(ACODEC_M4, row[0]);
    register_write(ACODEC_M5, row[1]);
    register_write(ACODEC_M6, row[2]);
    set_bits(ACODEC_M7, 0xFu << 4, (uint32_t)row[3] << 4);
    set_bits(ACODEC_MUX_ENABLE, 1u << 0, 0);
    set_bits(ACODEC_MUX_ENABLE, 1u << 2, 0);
    board_delay_ms(ACODEC_SETTLE_MS);
    set_bits(ACODEC_MUX_CONTROL, 1u << 2, 1u << 2);
}

static void start_i2s(uint32_t divider, uint32_t interrupts, bool transfer)
{
    cru_reset(RESET_I2S1, true);
    board_delay_ms(I2S_RESET_MS);
    cru_reset(RESET_I2S1, false);
    for (uint32_t index = 0; index < sizeof I2S_PINS / sizeof I2S_PINS[0]; index++) {
        gpio_mux(I2S_PINS[index], GPIO_FUNCTION_ALTERNATE);
    }
    register_write(GRF_PIN_GROUPS, GRF_I2S1_GROUP_A);
    cru_write(0x20, CRU_I2S_SOURCE);
    cru_write(0x20, CRU_I2S_DIVIDER);
    cru_write(0x20, CRU_I2S_OUTPUT);
    cru_write(0x20, CRU_I2S_CLOCK);
    register_write(I2S_XFER, 0);
    register_write(I2S_CLR, 1);
    board_delay_ms(I2S_CLEAR_MS);
    uint32_t start = hal_clock_milliseconds();
    while (register_read(I2S_CLR) != 0 && hal_clock_milliseconds() - start < I2C_TIMEOUT_MS) {
    }
    register_write(I2S_TXCR, I2S_FORMAT_16_BIT_STEREO);
    register_write(I2S_RXCR, I2S_FORMAT_16_BIT_STEREO);
    register_write(I2S_CKR, i2s_clock_register(divider));
    register_write(I2S_INTCR, interrupts);
    if (transfer) {
        register_write(I2S_XFER, 1);
    }
}

static void set_i2s_divider(uint32_t divider)
{
    register_write(I2S_XFER, 0);
    register_write(I2S_CKR, i2s_clock_register(divider));
    register_write(I2S_XFER, 1);
}

static bool feed(int16_t left, int16_t right)
{
    uint32_t start = hal_clock_milliseconds();
    while ((register_read(I2S_FIFOLR) & FIFO_LEVEL_MASK) >= FIFO_FULL_LEVEL) {
        if (hal_clock_milliseconds() - start >= FIFO_TIMEOUT_MS) {
            return false;
        }
    }
    register_write(I2S_TXDR, (uint32_t)(uint16_t)left | (uint32_t)(uint16_t)right << 16);
    return true;
}

static bool play_for(const int16_t *samples, int count, uint32_t duration_ms)
{
    uint32_t start = hal_clock_milliseconds();
    int index = 0;
    while (hal_clock_milliseconds() - start < duration_ms) {
        int16_t sample = samples != NULL ? samples[index] : 0;
        if (!feed(sample, sample)) {
            return false;
        }
        index = (index + 1) % count;
    }
    return true;
}

static void stop_i2s(void)
{
    register_write(I2S_XFER, 0);
    register_write(I2S_DMACR, 0);
    cru_reset(RESET_I2S1, true);
    for (uint32_t index = 0; index < sizeof I2S_PINS / sizeof I2S_PINS[0]; index++) {
        gpio_mux(I2S_PINS[index], GPIO_FUNCTION_GPIO);
        gpio_input(I2S_PINS[index]);
    }
}

static HalToneResult result_of(DacResult result)
{
    return result == DAC_NOT_READY ? HAL_TONE_NOT_READY : HAL_TONE_SETUP_FAILED;
}

static bool play_tone_segments(void)
{
    static int16_t samples[TONE_PERIOD_SAMPLES];
    tone_fill(samples, TONE_PERIOD_SAMPLES, TONE_AMPLITUDE);
    dac_output_on(&BUS);
    if (!play_for(samples, TONE_PERIOD_SAMPLES, TONE_SEGMENT_MS) ||
        !play_for(NULL, 1, TONE_GAP_MS)) {
        return false;
    }
    set_i2s_divider(i2s_divider_rounded(TONE_MCLK_HZ, TONE_RATE_HZ));
    return play_for(samples, TONE_PERIOD_SAMPLES, TONE_SEGMENT_MS) &&
           play_for(NULL, 1, TONE_GAP_MS);
}

static void use_dac_pins_as_gpio(void)
{
    for (uint32_t index = 0; index < sizeof DAC_PINS / sizeof DAC_PINS[0]; index++) {
        gpio_mux(DAC_PINS[index], GPIO_FUNCTION_GPIO);
    }
}

static HalToneResult start_audio_path(uint32_t divider, uint32_t interrupts, bool transfer)
{
    cru_gate(GATE_GPIO1, true);
    use_dac_pins_as_gpio();
    dac_power_pins(&BUS);
    start_soc_clocks();
    start_i2c();
    if (!dac_probe(&BUS)) {
        dac_output_off(&BUS);
        return HAL_TONE_NO_DAC;
    }
    start_acodec(ACODEC_44100);
    start_i2s(divider, interrupts, transfer);
    DacResult configured = dac_configure(&BUS, TONE_RATE_BYTE);
    if (configured != DAC_READY) {
        dac_output_off(&BUS);
        stop_i2s();
        return result_of(configured);
    }
    return HAL_TONE_PLAYED;
}

static void stop_audio_path(void)
{
    dac_output_off(&BUS);
    stop_i2s();
}

HalToneResult hal_audio_test_tone(void)
{
    hal_audio_dma_tone_stop();
    HalToneResult result =
        start_audio_path(i2s_divider_truncated(TONE_MCLK_HZ, TONE_RATE_HZ), I2S_INTERRUPTS, true);
    if (result != HAL_TONE_PLAYED) {
        return result;
    }
    bool played = play_tone_segments();
    stop_audio_path();
    return played ? HAL_TONE_PLAYED : HAL_TONE_STALLED;
}

static uint32_t dma_buffer[DMA_TONE_WORDS];
static DmaItem dma_item;
static bool dma_tone_active;

static void fill_dma_buffer(void)
{
    static int16_t samples[TONE_PERIOD_SAMPLES];
    tone_fill(samples, TONE_PERIOD_SAMPLES, TONE_AMPLITUDE);
    for (uint32_t index = 0; index < DMA_TONE_WORDS; index++) {
        uint16_t sample = (uint16_t)samples[index % TONE_PERIOD_SAMPLES];
        dma_buffer[index] = (uint32_t)sample | (uint32_t)sample << 16;
    }
}

HalToneResult hal_audio_dma_tone_start(void)
{
    hal_audio_dma_tone_stop();
    if (!dmac_present()) {
        return HAL_TONE_NO_DMAC;
    }
    HalToneResult result = start_audio_path(i2s_divider_rounded(TONE_MCLK_HZ, TONE_RATE_HZ),
                                            I2S_INTERRUPTS_FOR_DMA, false);
    if (result != HAL_TONE_PLAYED) {
        return result;
    }
    fill_dma_buffer();
    dma_item = dma_loop_item((uint32_t)(uintptr_t)dma_buffer, DMA_TONE_WORDS,
                             (uint32_t)(uintptr_t)&dma_item);
    dac_output_on(&BUS);
    if (!dmac_start_loop(&dma_item)) {
        stop_audio_path();
        return HAL_TONE_NO_DMAC;
    }
    register_write(I2S_DMACR, I2S_DMA_TX);
    register_write(I2S_XFER, 1);
    dma_tone_active = true;
    return HAL_TONE_PLAYED;
}

void hal_audio_dma_tone_stop(void)
{
    if (!dma_tone_active) {
        return;
    }
    dac_output_off(&BUS);
    dmac_stop();
    stop_i2s();
    dma_tone_active = false;
}

HalDmaToneStatus hal_audio_dma_tone_status(void)
{
    if (!dma_tone_active) {
        return (HalDmaToneStatus){.running = false, .position = 0};
    }
    uint32_t position =
        dma_loop_position(dmac_source(), (uint32_t)(uintptr_t)dma_buffer, DMA_TONE_WORDS);
    return (HalDmaToneStatus){.running = dmac_running(), .position = position};
}

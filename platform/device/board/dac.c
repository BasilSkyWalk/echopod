#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board.h"
#include "cru.h"
#include "dac.h"
#include "device/audio_math.h"
#include "device/dac_sequence.h"
#include "gpio.h"
#include "hal/audio.h"
#include "hal/clock.h"
#include "i2c.h"
#include "registers.h"

#define I2C0_BASE 0x40040000u
#define I2S1_BASE 0x40030000u
#define ACODEC_BASE 0x40090000u

#define I2S_TXCR (I2S1_BASE + 0x00u)
#define I2S_RXCR (I2S1_BASE + 0x04u)
#define I2S_CKR (I2S1_BASE + 0x08u)
#define I2S_FIFOLR (I2S1_BASE + 0x0Cu)
#define I2S_DMACR (I2S1_BASE + 0x10u)
#define I2S_INTCR (I2S1_BASE + 0x14u)
#define I2S_INTSR (I2S1_BASE + 0x18u)
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
#define I2S_FORMAT_16_BIT_STEREO 0x000Fu
#define I2S_FORMAT_32_BIT_WORDS 0x401Fu
#define I2S_INTERRUPTS 0x01F00102u
#define I2S_INTERRUPTS_FOR_DMA 0x01F00102u
#define I2S_UNDERRUN 0x00000002u
#define I2S_CLEAR_UNDERRUN 0x00000004u
#define I2S_DMA_TX 0x00000110u

enum {
    GATE_GPIO1 = 0x95,
    RESET_I2C0 = 0x1A,
    RESET_I2S1 = 0x04,
    I2C_TIMEOUT_MS = 10,
    FIFO_TIMEOUT_MS = 10,
    I2C_BUS_KHZ = 100,
    I2C_AUTO_INCREMENT = 0x01,
    FRAME_CAPACITY = 8,
    FIFO_LEVEL_MASK = 0x3F,
    FIFO_FULL_LEVEL = 0x1E,
    TONE_RATE_HZ = 44100,
    TONE_MCLK_HZ = 11288960,
    TONE_PERIOD_SAMPLES = 100,
    TONE_AMPLITUDE = 9000,
    DURATION_CHECK_SAMPLES = 64,
    TONE_SEGMENT_MS = 2000,
    TONE_GAP_MS = 400,
    I2S_RESET_MS = 1,
    I2S_CLEAR_MS = 2,
    ACODEC_SETTLE_MS = 5,
    FADE_STEP_US = 2000
};

static const uint32_t SOC_AUDIO_GATES[] = {0x0E, 0x10, 0x11, 0x12, 0x13, 0x82, 0x83, 0x96};
typedef struct AudioRate {
    uint32_t rate;
    uint32_t mclk;
    uint8_t acodec[4];
    uint8_t dac_byte;
} AudioRate;

static const AudioRate RATES[] = {
    {44100, 11288960, {0x06, 0x24, 0x19, 0x03}, 1},
    {48000, 12288000, {0x00, 0x30, 0x19, 0x0A}, 2},
    {88200, 11288960, {0x06, 0x24, 0x19, 0x03}, 3},
    {96000, 24576000, {0x00, 0x30, 0x19, 0x09}, 4},
    {176400, 11288960, {0x06, 0x24, 0x19, 0x03}, 5},
    {192000, 24576000, {0x00, 0x30, 0x19, 0x09}, 6},
};
static const GpioPin I2C_PINS[] = {{2, 10}, {2, 11}};
static const GpioPin I2S_PINS[] = {{1, 0}, {1, 1}, {1, 2}, {1, 3}};
static const GpioPin DAC_PINS[] = {
    [DAC_PIN_MUTE] = {2, 0},  [DAC_PIN_ENABLE_A] = {2, 14}, [DAC_PIN_ENABLE_B] = {2, 2},
    [DAC_PIN_RESET] = {2, 1}, [DAC_PIN_AMP] = {1, 4},
};

static I2cPort port;
static uint8_t volume_value;
static bool volume_known;
static bool gain_high;
static uint8_t filter_choice;
static uint8_t balance;
static bool balance_known;
static bool dac_up;
static bool muted;
static uint8_t applied_volume;
static void (*fade_service)(void);
static bool fades_off;
static bool amp_on;

bool dac_path_is_up(void)
{
    return dac_up;
}

bool dac_path_amp_on(void)
{
    return amp_on;
}

HalToneDetail hal_audio_tone_detail(void)
{
    return (HalToneDetail){.stage = port.stage, .pending = port.pending};
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
    return i2c_send(&port, frame, length);
}

static bool dac_read(void *context, uint8_t address, uint32_t map, uint8_t *value)
{
    (void)context;
    uint8_t frame[FRAME_CAPACITY];
    return i2c_receive(&port, frame, frame_header(frame, address, map), value, 1);
}

static void dac_set_pin(void *context, DacPin pin, bool level)
{
    (void)context;
    if (pin == DAC_PIN_MUTE || pin == DAC_PIN_ENABLE_A) {
        level = board_bluetooth_pin(pin == DAC_PIN_MUTE, level);
    }
    if (pin == DAC_PIN_AMP) {
        amp_on = level;
    }
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
    i2c_port_start(&port, I2C0_BASE, I2C_TIMEOUT_MS, I2C_BUS_KHZ);
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

static void start_i2s(uint32_t divider, uint32_t format, uint32_t interrupts, bool transfer)
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
    register_write(I2S_TXCR, format);
    register_write(I2S_RXCR, format);
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

static bool fifo_full(void)
{
    return (register_read(I2S_FIFOLR) & FIFO_LEVEL_MASK) >= FIFO_FULL_LEVEL;
}

static bool feed(int16_t left, int16_t right)
{
    if (fifo_full()) {
        uint32_t start = hal_clock_milliseconds();
        while (fifo_full()) {
            if (hal_clock_milliseconds() - start >= FIFO_TIMEOUT_MS) {
                return false;
            }
        }
    }
    register_write(I2S_TXDR, (uint32_t)(uint16_t)left | (uint32_t)(uint16_t)right << 16);
    return true;
}

static bool play_for(const int16_t *samples, int count, uint32_t duration_ms)
{
    uint32_t start = hal_clock_milliseconds();
    int index = 0;
    for (uint32_t fed = 0;; fed++) {
        if (fed % DURATION_CHECK_SAMPLES == 0 && hal_clock_milliseconds() - start >= duration_ms) {
            return true;
        }
        int16_t sample = samples != NULL ? samples[index] : 0;
        if (!feed(sample, sample)) {
            return false;
        }
        index = index + 1 < count ? index + 1 : 0;
    }
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

static uint8_t current_balance(void)
{
    return balance_known ? balance : DAC_BALANCE_CENTER;
}

static uint8_t current_volume(void)
{
    return volume_known ? volume_value : DAC_VOLUME_MUTED;
}

static bool play_tone_segments(void)
{
    static int16_t samples[TONE_PERIOD_SAMPLES];
    tone_fill(samples, TONE_PERIOD_SAMPLES, TONE_AMPLITUDE);
    dac_output_on(&BUS, current_volume());
    applied_volume = current_volume();
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

static const AudioRate *rate_of(uint32_t rate)
{
    for (uint32_t index = 0; index < sizeof RATES / sizeof RATES[0]; index++) {
        if (RATES[index].rate == rate) {
            return &RATES[index];
        }
    }
    return NULL;
}

static HalToneResult start_audio_path(const AudioRate *rate, uint32_t divider, uint32_t format,
                                      uint32_t interrupts, bool transfer)
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
    start_acodec(rate->acodec);
    start_i2s(divider, format, interrupts, transfer);
    DacResult configured = dac_configure(&BUS, rate->dac_byte);
    if (configured != DAC_READY) {
        dac_output_off(&BUS);
        stop_i2s();
        return result_of(configured);
    }
    if (gain_high) {
        dac_set_gain(&BUS, true);
    }
    if (filter_choice != 0) {
        dac_set_filter(&BUS, filter_choice);
    }
    dac_up = true;
    muted = true;
    applied_volume = DAC_VOLUME_MUTED;
    return HAL_TONE_PLAYED;
}

static void stop_audio_path(void)
{
    dac_up = false;
    dac_output_off(&BUS);
    applied_volume = DAC_VOLUME_MUTED;
    stop_i2s();
}

HalToneResult hal_audio_test_tone(void)
{
    hal_audio_dma_tone_stop();
    const AudioRate *rate = rate_of(TONE_RATE_HZ);
    HalToneResult result = start_audio_path(rate, i2s_divider_truncated(rate->mclk, rate->rate),
                                            I2S_FORMAT_16_BIT_STEREO, I2S_INTERRUPTS, true);
    if (result != HAL_TONE_PLAYED) {
        return result;
    }
    bool played = play_tone_segments();
    stop_audio_path();
    return played ? HAL_TONE_PLAYED : HAL_TONE_STALLED;
}

HalToneResult dac_path_start(uint32_t sample_rate)
{
    const AudioRate *rate = rate_of(sample_rate);
    if (rate == NULL) {
        return HAL_TONE_SETUP_FAILED;
    }
    return start_audio_path(rate, i2s_divider_rounded(rate->mclk, rate->rate),
                            I2S_FORMAT_32_BIT_WORDS, I2S_INTERRUPTS_FOR_DMA, false);
}

void dac_path_stop(void)
{
    stop_audio_path();
}

void dac_path_stop_if_up(void)
{
    if (dac_up) {
        stop_audio_path();
    }
}

void dac_path_set_fade_service(void (*service)(void))
{
    fade_service = service;
}

static void wait_serving(uint32_t microseconds)
{
    uint32_t start = hal_clock_microseconds();
    do {
        if (fade_service != NULL) {
            fade_service();
        }
    } while (hal_clock_microseconds() - start < microseconds);
}

void dac_path_set_fades(bool fades)
{
    fades_off = !fades;
}

static void fade_to(uint8_t target)
{
    while (applied_volume != target) {
        applied_volume = fades_off ? target : dac_ramp_next(applied_volume, target);
        dac_set_balanced_volume(&BUS, applied_volume, current_balance());
        if (!fades_off) {
            wait_serving(FADE_STEP_US);
        }
    }
}

void dac_path_output_on(void)
{
    dac_output_on(&BUS, DAC_VOLUME_MUTED);
    applied_volume = DAC_VOLUME_MUTED;
    muted = true;
}

void dac_path_set_muted(bool mute)
{
    muted = mute;
    if (mute) {
        fade_to(DAC_VOLUME_MUTED);
        dac_set_muted(&BUS, true, DAC_VOLUME_MUTED);
        return;
    }
    dac_set_muted(&BUS, false, applied_volume);
    fade_to(current_volume());
}

void dac_path_set_volume(uint8_t value)
{
    volume_value = value;
    volume_known = true;
    if (dac_up && !muted) {
        fade_to(value);
    }
}

void dac_path_set_gain(bool high)
{
    gain_high = high;
    if (dac_up) {
        dac_set_gain(&BUS, high);
    }
}

void dac_path_set_filter(uint8_t filter)
{
    filter_choice = filter;
    if (dac_up) {
        dac_set_filter(&BUS, filter);
    }
}

void dac_path_set_balance(uint8_t balance_step)
{
    balance = balance_step;
    balance_known = true;
    if (dac_up && !muted) {
        dac_set_balanced_volume(&BUS, applied_volume, current_balance());
    }
}

bool dac_path_take_underrun(void)
{
    if (!dac_up || (register_read(I2S_INTSR) & I2S_UNDERRUN) == 0) {
        return false;
    }
    register_write(I2S_INTCR, I2S_INTERRUPTS_FOR_DMA | I2S_CLEAR_UNDERRUN);
    return true;
}

void dac_path_start_transfer(void)
{
    register_write(I2S_DMACR, I2S_DMA_TX);
    register_write(I2S_XFER, 1);
}

void dac_path_stop_transfer(void)
{
    register_write(I2S_XFER, 0);
    register_write(I2S_DMACR, 0);
}

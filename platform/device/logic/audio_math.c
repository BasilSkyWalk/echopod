#include "device/audio_math.h"

enum {
    HZ_PER_KHZ = 1000,
    I2C_CYCLES_PER_BIT = 8,
    I2C_DIVIDER_OFFSET = 2,
    HIGH_HALF_SHIFT = 16,
    BIT_CLOCKS_PER_FRAME = 64,
    I2S_DIVIDER_SHIFT = 16,
    I2S_FRAME_BITS = 0x3F3F,
    SINE_FRACTION_BITS = 30
};

uint32_t i2c_clock_divider(uint32_t pclk_hz, uint32_t bus_khz)
{
    uint32_t pclk_khz = pclk_hz / HZ_PER_KHZ;
    uint32_t per_bit = I2C_CYCLES_PER_BIT * bus_khz;
    uint32_t divider = (pclk_khz + per_bit - 1) / per_bit - I2C_DIVIDER_OFFSET;
    return divider / 2 + ((divider + 1) / 2 << HIGH_HALF_SHIFT);
}

uint32_t i2s_divider_truncated(uint32_t mclk_hz, uint32_t rate_hz)
{
    return mclk_hz / (BIT_CLOCKS_PER_FRAME * rate_hz) - 1;
}

uint32_t i2s_divider_rounded(uint32_t mclk_hz, uint32_t rate_hz)
{
    uint32_t frame_clocks = BIT_CLOCKS_PER_FRAME * rate_hz;
    return (mclk_hz + frame_clocks / 2) / frame_clocks - 1;
}

uint32_t i2s_clock_register(uint32_t divider)
{
    return divider << I2S_DIVIDER_SHIFT | I2S_FRAME_BITS;
}

static int64_t fixed_one(void)
{
    return INT64_C(1) << SINE_FRACTION_BITS;
}

static int64_t fixed_multiply(int64_t first, int64_t second)
{
    return first * second / fixed_one();
}

static int64_t step_angle(int count)
{
    return INT64_C(6283185307) * fixed_one() / (INT64_C(1000000000) * count);
}

static int64_t series(int64_t angle, int first_power)
{
    int64_t angle_squared = fixed_multiply(angle, angle);
    int64_t term = first_power == 0 ? fixed_one() : angle;
    int64_t sum = term;
    for (int power = first_power + 2; power <= first_power + 12; power += 2) {
        term = -fixed_multiply(term, angle_squared) / ((power - 1) * power);
        sum += term;
    }
    return sum;
}

static int16_t to_sample(int64_t value, int16_t amplitude)
{
    int64_t scaled = value * amplitude;
    int64_t half = fixed_one() / 2;
    return (int16_t)((scaled + (scaled >= 0 ? half : -half)) / fixed_one());
}

void tone_fill(int16_t *samples, int count, int16_t amplitude)
{
    int64_t angle = step_angle(count);
    int64_t cosine = series(angle, 0);
    int64_t sine = series(angle, 1);
    int64_t x = fixed_one();
    int64_t y = 0;
    for (int index = 0; index < count; index++) {
        samples[index] = to_sample(y, amplitude);
        int64_t next_x = fixed_multiply(x, cosine) - fixed_multiply(y, sine);
        y = fixed_multiply(x, sine) + fixed_multiply(y, cosine);
        x = next_x;
    }
}

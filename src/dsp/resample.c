#include "dsp/resample.h"

enum {
    TAP_BITS = 14,
    TAP_HALF = 1 << (TAP_BITS - 1),
    SAMPLE_MAX = 8388607,
    SAMPLE_MIN = -8388608,
    START_POSITION = RESAMPLE_HISTORY * RESAMPLE_PHASES,
    UNITY_QUARTERS = 4
};

static const int16_t TAPS[5][RESAMPLE_PHASES][RESAMPLE_TAPS] = {
    {
        {98, -192, 314, -453, 591, -710, 790, 15543, 790, -710, 591, -453, 314, -192, 98, -35},
        {30, -39, 15, 79, -310, 827, -2208, 14132, 5366, -2362, 1399, -863, 513, -279, 131, -47},
        {-26, 87, -214, 449, -855, 1582, -3153, 10321, 10323, -3153, 1582, -855, 449, -214, 87,
         -26},
        {-47, 131, -279, 513, -863, 1399, -2362, 5366, 14132, -2208, 827, -310, 79, 15, -39, 30},
    },
    {
        {-14, 201, -423, 284, 628, -2258, 3893, 11788, 3893, -2258, 628, 284, -423, 201, -14, -26},
        {-56, 200, -245, -127, 1002, -1838, 1229, 11162, 6783, -1975, -94, 712, -497, 120, 58, -50},
        {-65, 140, -41, -404, 1002, -1007, -807, 9374, 9374, -807, -1007, 1002, -404, -41, 140,
         -65},
        {-50, 58, 120, -497, 712, -94, -1975, 6783, 11162, 1229, -1838, 1002, -127, -245, 200, -56},
    },
    {
        {65, -227, 0, 734, -767, -1353, 4811, 9836, 4811, -1353, -767, 734, 0, -227, 65, 22},
        {78, -141, -174, 667, -179, -1760, 2809, 9446, 6745, -423, -1328, 622, 233, -278, 21, 46},
        {68, -51, -266, 477, 310, -1710, 1001, 8361, 8365, 1001, -1710, 310, 477, -266, -51, 68},
        {46, 21, -278, 233, 622, -1328, -423, 6745, 9446, 2809, -1760, -179, 667, -174, -141, 78},
    },
    {
        {-105, -63, 435, 138, -1295, -207, 5061, 8443, 5061, -207, -1295, 138, 435, -63, -105, 13},
        {-66, -125, 312, 373, -984, -939, 3589, 8203, 6413, 839, -1441, -196, 506, 38, -136, -2},
        {-30, -147, 170, 493, -591, -1342, 2138, 7502, 7500, 2138, -1342, -591, 493, 170, -147,
         -30},
        {-2, -136, 38, 506, -196, -1441, 839, 6413, 8203, 3589, -939, -984, 373, 312, -125, -66},
    },
    {
        {-50, 192, 314, -452, -1159, 709, 4980, 7351, 4980, 709, -1159, -452, 314, 192, -50, -35},
        {-64, 111, 348, -183, -1133, -77, 3890, 7192, 5950, 1673, -1010, -730, 214, 270, -16, -51},
        {-63, 39, 328, 46, -974, -654, 2759, 6712, 6710, 2759, -654, -974, 46, 328, 39, -63},
        {-51, -16, 270, 214, -730, -1010, 1673, 5950, 7192, 3890, -77, -1133, -183, 348, 111, -64},
    },
};

void resample_configure(Resample *resample, SpeedFifo *fifo, uint32_t quarters)
{
    speed_fifo_reset(fifo, RESAMPLE_LIMIT);
    speed_fifo_pad(fifo, RESAMPLE_HISTORY);
    fifo->real = RESAMPLE_HISTORY;
    resample->quarters = quarters;
    resample->position = START_POSITION;
    resample->taps = TAPS[quarters < UNITY_QUARTERS ? 0 : quarters - UNITY_QUARTERS];
}

static bool ready(const Resample *resample, SpeedFifo *fifo)
{
    uint32_t whole = resample->position / RESAMPLE_PHASES;
    if (fifo->finished) {
        if (whole >= fifo->real) {
            return false;
        }
        speed_fifo_pad(fifo, whole + RESAMPLE_AHEAD + 1);
    }
    return whole + RESAMPLE_AHEAD < fifo->count;
}

static int32_t filtered(const int16_t *row, const int32_t *first)
{
    int64_t sum = TAP_HALF;
    for (uint32_t tap = 0; tap < RESAMPLE_TAPS; tap++) {
        sum += (int64_t)row[tap] * first[SPEED_CHANNELS * tap];
    }
    sum >>= TAP_BITS;
    return sum > SAMPLE_MAX ? SAMPLE_MAX : sum < SAMPLE_MIN ? SAMPLE_MIN : (int32_t)sum;
}

static void frame_at(const Resample *resample, const SpeedFifo *fifo, int32_t *frame)
{
    const int16_t *row = resample->taps[resample->position % RESAMPLE_PHASES];
    const int32_t *first =
        speed_fifo_at(fifo, resample->position / RESAMPLE_PHASES - RESAMPLE_HISTORY);
    for (uint32_t channel = 0; channel < SPEED_CHANNELS; channel++) {
        frame[channel] = filtered(row, first + channel);
    }
}

uint32_t resample_pull(Resample *resample, SpeedFifo *fifo, int32_t *frames, uint32_t capacity)
{
    uint32_t made = 0;
    while (made < capacity && ready(resample, fifo)) {
        frame_at(resample, fifo, frames + SPEED_CHANNELS * made);
        resample->position += resample->quarters;
        made++;
    }
    uint32_t whole = resample->position / RESAMPLE_PHASES;
    if (whole > RESAMPLE_HISTORY) {
        speed_fifo_drop(fifo, whole - RESAMPLE_HISTORY);
        resample->position -= RESAMPLE_PHASES * (whole - RESAMPLE_HISTORY);
    }
    return made;
}

bool resample_drained(const Resample *resample, const SpeedFifo *fifo)
{
    return fifo->finished && resample->position / RESAMPLE_PHASES >= fifo->real;
}

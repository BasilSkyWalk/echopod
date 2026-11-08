#include "ui/cover_flow.h"

#include <stdbool.h>

enum {
    MIN_UNITS_PER_MS = 1
};

typedef struct CoverShape {
    int center_x;
    int width;
    int near_height;
    int far_height;
} CoverShape;

static int32_t abs_of(int32_t value)
{
    return value < 0 ? -value : value;
}

static int lerp(int from, int to, int32_t amount)
{
    return from + (int)((int32_t)(to - from) * amount / COVER_FLOW_ONE);
}

static int32_t depth_scale(int shrink, int32_t beyond)
{
    int32_t scale = COVER_FLOW_ONE - shrink * beyond / COVER_FLOW_ONE;
    return scale < 0 ? 0 : scale;
}

static CoverShape shape_at(const CoverFlowLayout *layout, int32_t distance)
{
    if (distance >= COVER_FLOW_ONE) {
        int32_t beyond = distance - COVER_FLOW_ONE;
        int offset = layout->side_offset + (int)(layout->side_spacing * beyond / COVER_FLOW_ONE);
        int32_t scale = depth_scale(layout->side_shrink, beyond);
        return (CoverShape){offset, lerp(0, layout->side_width, scale),
                            lerp(0, layout->side_near_height, scale),
                            lerp(0, layout->side_far_height, scale)};
    }
    return (CoverShape){
        lerp(0, layout->side_offset, distance),
        lerp(layout->center_size, layout->side_width, distance),
        lerp(layout->center_size, layout->side_near_height, distance),
        lerp(layout->center_size, layout->side_far_height, distance),
    };
}

Quad cover_flow_quad(const CoverFlowLayout *layout, int32_t relative)
{
    CoverShape shape = shape_at(layout, abs_of(relative));
    bool right = relative > 0;
    int center_x = layout->center_x + (right ? shape.center_x : -shape.center_x);
    int middle = layout->center_top + layout->center_size / 2;
    int left_height = right ? shape.near_height : shape.far_height;
    int right_height = right ? shape.far_height : shape.near_height;
    return (Quad){
        .left = center_x - shape.width / 2,
        .width = shape.width,
        .left_top = middle - left_height / 2,
        .left_height = left_height,
        .right_top = middle - right_height / 2,
        .right_height = right_height,
    };
}

Quad cover_flow_stacked_quad(const CoverStackLayout *layout, int32_t relative)
{
    int32_t distance = abs_of(relative);
    int offset = lerp(0, layout->side_offset, distance);
    int width = lerp(layout->center_size, layout->side_width, distance);
    int height = lerp(layout->center_size, layout->side_height, distance);
    if (distance >= COVER_FLOW_ONE) {
        int32_t beyond = distance - COVER_FLOW_ONE;
        int32_t scale = depth_scale(layout->side_shrink, beyond);
        offset = layout->side_offset + (int)(layout->side_spacing * beyond / COVER_FLOW_ONE);
        width = lerp(0, layout->side_width, scale);
        height = lerp(0, layout->side_height, scale);
    }
    int middle = layout->center_top + layout->center_size / 2 + (relative > 0 ? offset : -offset);
    int top = middle - height / 2;
    return (Quad){
        .left = layout->center_x - width / 2,
        .width = width,
        .left_top = top,
        .left_height = height,
        .right_top = top,
        .right_height = height,
    };
}

static int fade(int light, const CoverFlowLayout *layout, int32_t distance)
{
    if (distance <= layout->fade_start) {
        return light;
    }
    if (distance >= layout->fade_end) {
        return 0;
    }
    return (int)(light * (layout->fade_end - distance) / (layout->fade_end - layout->fade_start));
}

QuadLight cover_flow_light(const CoverFlowLayout *layout, int32_t relative)
{
    int32_t distance = abs_of(relative);
    int32_t turn = distance > COVER_FLOW_ONE ? COVER_FLOW_ONE : distance;
    uint16_t near =
        (uint16_t)fade(lerp(COLOR_LIGHT_FULL, layout->side_near_light, turn), layout, distance);
    uint16_t far =
        (uint16_t)fade(lerp(COLOR_LIGHT_FULL, layout->side_far_light, turn), layout, distance);
    return relative > 0 ? (QuadLight){near, far} : (QuadLight){far, near};
}

enum {
    APPROACH_STEPS_PER_CONSTANT = 16,
    APPROACH_TABLE_SIZE = 65,
    APPROACH_SHIFT = 16
};

static const uint16_t APPROACH_FRACTIONS[APPROACH_TABLE_SIZE] = {
    0,     3971,  7701,  11205, 14497, 17589, 20494, 23223, 25786, 28195, 30457, 32582, 34579,
    36455, 38217, 39872, 41427, 42887, 44260, 45549, 46760, 47897, 48966, 49970, 50913, 51799,
    52631, 53413, 54148, 54838, 55486, 56095, 56667, 57204, 57709, 58183, 58629, 59047, 59440,
    59810, 60156, 60482, 60789, 61076, 61346, 61600, 61839, 62063, 62273, 62471, 62657, 62831,
    62995, 63149, 63293, 63429, 63557, 63677, 63790, 63895, 63995, 64088, 64176, 64258, 64336};

static uint32_t approach_fraction(uint32_t elapsed_ms, uint32_t time_constant_ms)
{
    uint64_t scaled =
        ((uint64_t)elapsed_ms * APPROACH_STEPS_PER_CONSTANT << APPROACH_SHIFT) / time_constant_ms;
    uint64_t index = scaled >> APPROACH_SHIFT;
    if (index >= APPROACH_TABLE_SIZE - 1) {
        return 1u << APPROACH_SHIFT;
    }
    uint32_t low = APPROACH_FRACTIONS[index];
    uint32_t high = APPROACH_FRACTIONS[index + 1];
    uint64_t within = scaled & ((1u << APPROACH_SHIFT) - 1u);
    return low + (uint32_t)(((high - low) * within) >> APPROACH_SHIFT);
}

int32_t cover_flow_approach(int32_t position, int32_t target, uint32_t elapsed_ms,
                            uint32_t time_constant_ms)
{
    int32_t difference = target - position;
    int32_t distance = abs_of(difference);
    uint32_t fraction = approach_fraction(elapsed_ms, time_constant_ms);
    int32_t step = (int32_t)(((int64_t)distance * fraction) >> APPROACH_SHIFT);
    int32_t minimum = (int32_t)elapsed_ms * MIN_UNITS_PER_MS;
    step = step < minimum ? minimum : step;
    step = step > distance ? distance : step;
    return difference < 0 ? position - step : position + step;
}

static void insert_by_distance(int *albums, int count, int32_t position)
{
    int album = albums[count];
    int32_t distance = abs_of(album * COVER_FLOW_ONE - position);
    int slot = count;
    while (slot > 0 && abs_of(albums[slot - 1] * COVER_FLOW_ONE - position) < distance) {
        albums[slot] = albums[slot - 1];
        slot--;
    }
    albums[slot] = album;
}

int cover_flow_draw_order(int32_t position, int count, int *albums)
{
    int center = (int)((position + COVER_FLOW_ONE / 2) / COVER_FLOW_ONE);
    int found = 0;
    for (int album = center - COVER_FLOW_REACH; album <= center + COVER_FLOW_REACH; album++) {
        if (album < 0 || album >= count) {
            continue;
        }
        albums[found] = album;
        insert_by_distance(albums, found, position);
        found++;
    }
    return found;
}

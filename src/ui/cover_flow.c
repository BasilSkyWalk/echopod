#include "ui/cover_flow.h"

#include <stdbool.h>

enum {
    TIME_CONSTANT_MS = 70,
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

static CoverShape shape_at(const CoverFlowLayout *layout, int32_t distance)
{
    if (distance >= COVER_FLOW_ONE) {
        int32_t beyond = distance - COVER_FLOW_ONE;
        int offset = layout->side_offset + (int)(layout->side_spacing * beyond / COVER_FLOW_ONE);
        return (CoverShape){offset, layout->side_width, layout->side_near_height,
                            layout->side_far_height};
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

int32_t cover_flow_approach(int32_t position, int32_t target, uint32_t elapsed_ms)
{
    int32_t difference = target - position;
    int32_t distance = abs_of(difference);
    int32_t step = (int32_t)((int64_t)distance * elapsed_ms / TIME_CONSTANT_MS);
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

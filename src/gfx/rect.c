#include "gfx/rect.h"

static int max_of(int first, int second)
{
    return first > second ? first : second;
}

static int min_of(int first, int second)
{
    return first < second ? first : second;
}

bool rect_is_empty(Rect rect)
{
    return rect.width <= 0 || rect.height <= 0;
}

Rect rect_intersection(Rect first, Rect second)
{
    int left = max_of(first.x, second.x);
    int top = max_of(first.y, second.y);
    int right = min_of(first.x + first.width, second.x + second.width);
    int bottom = min_of(first.y + first.height, second.y + second.height);
    return (Rect){left, top, right - left, bottom - top};
}

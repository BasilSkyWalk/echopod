#include "gfx/strip_renderer.h"

static int min_of(int first, int second)
{
    return first < second ? first : second;
}

static void render_strip(const StripRenderer *renderer, Rect strip, FrameDrawer drawer,
                         FrameSink sink)
{
    Canvas canvas = canvas_make(renderer->strip_pixels, strip);
    if (!renderer->drawer_paints_everything) {
        canvas_clear(&canvas, renderer->background);
    }
    drawer.draw(drawer.context, &canvas);
    sink.write(sink.context, strip, renderer->strip_pixels);
}

void strip_renderer_render(const StripRenderer *renderer, FrameDrawer drawer, FrameSink sink)
{
    for (int top = 0; top < renderer->screen_height; top += renderer->strip_height) {
        int height = min_of(renderer->strip_height, renderer->screen_height - top);
        Rect strip = {0, top, renderer->screen_width, height};
        render_strip(renderer, strip, drawer, sink);
    }
}

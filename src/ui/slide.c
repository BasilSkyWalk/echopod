#include "ui/slide.h"

static uint32_t elapsed_of(const Slide *slide, uint32_t now_ms)
{
    uint32_t elapsed = now_ms - slide->start_ms;
    return elapsed > slide->duration_ms ? slide->duration_ms : elapsed;
}

void slide_start(Slide *slide, uint32_t now_ms, SlideDirection direction, uint32_t duration_ms)
{
    *slide = (Slide){
        .active = true, .start_ms = now_ms, .duration_ms = duration_ms, .direction = direction};
}

int slide_progress(const Slide *slide, uint32_t now_ms, int width)
{
    uint64_t duration = slide->duration_ms;
    uint64_t remaining = duration - elapsed_of(slide, now_ms);
    uint64_t left =
        (uint64_t)width * remaining * remaining * remaining / (duration * duration * duration);
    return width - (int)left;
}

bool slide_finished(const Slide *slide, uint32_t now_ms)
{
    return elapsed_of(slide, now_ms) >= slide->duration_ms;
}

int slide_leaving_x(const Slide *slide, uint32_t now_ms, int width)
{
    int progress = slide_progress(slide, now_ms, width);
    return slide->direction == SLIDE_FORWARD ? -progress : progress;
}

int slide_arriving_x(const Slide *slide, uint32_t now_ms, int width)
{
    int still_to_go = width - slide_progress(slide, now_ms, width);
    return slide->direction == SLIDE_FORWARD ? still_to_go : -still_to_go;
}

bool slide_shows_arriving_title(const Slide *slide, uint32_t now_ms)
{
    return elapsed_of(slide, now_ms) >= slide->duration_ms / 2;
}

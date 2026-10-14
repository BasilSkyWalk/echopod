#include <SDL.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "app/app.h"
#include "core/deadline.h"
#include "hal/clock.h"
#include "sim_display.h"
#include "sim_input.h"
#include "sim_options.h"

enum {
    EXIT_USAGE = 2,
    FRAME_PERIOD_MS = 16,
    TITLE_REFRESH_MS = 500,
    TITLE_CAPACITY = 128
};

static const double MICROSECONDS_PER_MILLISECOND = 1000.0;

static void print_usage(void)
{
    fputs("usage: echo_sim [--scale N] [--frames N] [--headless] [--screenshot FILE.bmp]\n",
          stderr);
}

static bool start_sdl(const SimOptions *options)
{
    Uint32 subsystems = options->headless ? SDL_INIT_TIMER : SDL_INIT_VIDEO | SDL_INIT_TIMER;
    if (SDL_Init(subsystems) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    if (!options->headless && !sim_display_open(options->scale)) {
        SDL_Log("could not open the display: %s", SDL_GetError());
        return false;
    }
    return true;
}

static bool pump_events(void)
{
    bool keep_running = true;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            keep_running = false;
        }
        sim_input_handle_event(&event);
    }
    return keep_running;
}

static void format_frame_stats(char *text, size_t capacity)
{
    const FrameStats *stats = app_frame_stats();
    snprintf(text, capacity, "frame %.2f ms avg, %.2f ms worst, %" PRIu32 " fps achievable",
             frame_stats_average_us(stats) / MICROSECONDS_PER_MILLISECOND,
             frame_stats_worst_us(stats) / MICROSECONDS_PER_MILLISECOND,
             frame_stats_achievable_fps(stats));
}

static void refresh_title(void)
{
    char stats_text[TITLE_CAPACITY];
    char title[TITLE_CAPACITY * 2];
    format_frame_stats(stats_text, sizeof stats_text);
    snprintf(title, sizeof title, "Echo Mini | %s", stats_text);
    sim_display_set_title(title);
}

static void wait_for_next_frame(uint32_t frame_started_ms)
{
    uint32_t elapsed_ms = hal_clock_milliseconds() - frame_started_ms;
    if (elapsed_ms < FRAME_PERIOD_MS) {
        SDL_Delay(FRAME_PERIOD_MS - elapsed_ms);
    }
}

static bool frame_limit_reached(const SimOptions *options, uint32_t frames)
{
    return options->frame_limit > 0 && frames >= options->frame_limit;
}

static void run(const SimOptions *options)
{
    uint32_t frames = 0;
    uint32_t next_title_ms = hal_clock_milliseconds();
    while (pump_events() && !frame_limit_reached(options, frames)) {
        uint32_t frame_started_ms = hal_clock_milliseconds();
        app_step();
        frames++;
        if (options->headless) {
            continue;
        }
        if (deadline_reached(frame_started_ms, next_title_ms)) {
            refresh_title();
            next_title_ms = frame_started_ms + TITLE_REFRESH_MS;
        }
        wait_for_next_frame(frame_started_ms);
    }
}

static int finish(const SimOptions *options)
{
    int status = EXIT_SUCCESS;
    if (options->screenshot_path != NULL && !sim_display_save_bmp(options->screenshot_path)) {
        SDL_Log("could not save %s: %s", options->screenshot_path, SDL_GetError());
        status = EXIT_FAILURE;
    }
    char stats_text[TITLE_CAPACITY];
    format_frame_stats(stats_text, sizeof stats_text);
    printf("%s\n", stats_text);
    sim_display_close();
    SDL_Quit();
    return status;
}

int main(int argument_count, char **arguments)
{
    SimOptions options;
    if (!sim_options_parse(argument_count, arguments, &options)) {
        print_usage();
        return EXIT_USAGE;
    }
    SDL_SetMainReady();
    if (!start_sdl(&options)) {
        SDL_Quit();
        return EXIT_FAILURE;
    }
    app_start();
    run(&options);
    return finish(&options);
}

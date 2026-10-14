#include "app/app.h"

#include <stddef.h>

#include "core/key_tracker.h"
#include "gfx/strip_renderer.h"
#include "hal/clock.h"
#include "hal/display.h"
#include "hal/input.h"
#include "screen.h"
#include "test_pattern_screen.h"

enum {
    STRIP_HEIGHT = 34
};

static const KeyConfig KEY_CONFIG = {
    .timing =
        {
            .long_press_ms = 600,
            .repeat_delay_ms = 400,
            .repeat_interval_ms = 120,
        },
    .behaviors =
        {
            [KEY_VOLUME_UP] = KEY_BEHAVIOR_REPEAT,
            [KEY_VOLUME_DOWN] = KEY_BEHAVIOR_REPEAT,
            [KEY_PLAY] = KEY_BEHAVIOR_TAP_OR_HOLD,
            [KEY_MENU] = KEY_BEHAVIOR_TAP_OR_HOLD,
            [KEY_BACK] = KEY_BEHAVIOR_TAP_OR_HOLD,
            [KEY_POWER] = KEY_BEHAVIOR_TAP_OR_HOLD,
        },
};

typedef struct AppState {
    KeyTracker keys;
    FrameStats frame_stats;
    TestPatternScreen test_pattern;
    Screen screen;
} AppState;

static AppState app;
static Color strip_pixels[HAL_DISPLAY_WIDTH * STRIP_HEIGHT];

static void read_input(void)
{
    HalKeyTransition transition;
    while (hal_input_next_transition(&transition)) {
        if (transition.pressed) {
            key_tracker_key_down(&app.keys, transition.key, transition.time_ms);
        } else {
            key_tracker_key_up(&app.keys, transition.key, transition.time_ms);
        }
    }
}

static void dispatch_key_events(uint32_t now_ms)
{
    KeyEvent event;
    while (key_tracker_next_event(&app.keys, &event)) {
        app.screen.handle_key(app.screen.context, event, now_ms);
    }
}

static void draw_current_screen(void *context, Canvas *canvas)
{
    const Screen *screen = context;
    screen->draw(screen->context, canvas);
}

static void write_to_display(void *context, Rect area, const Color *pixels)
{
    (void)context;
    hal_display_write(area.x, area.y, area.width, area.height, pixels);
}

static void render_frame(void)
{
    StripRenderer renderer = {
        .strip_pixels = strip_pixels,
        .screen_width = HAL_DISPLAY_WIDTH,
        .screen_height = HAL_DISPLAY_HEIGHT,
        .strip_height = STRIP_HEIGHT,
        .background = 0,
    };
    FrameDrawer drawer = {.draw = draw_current_screen, .context = &app.screen};
    FrameSink sink = {.write = write_to_display, .context = NULL};
    strip_renderer_render(&renderer, drawer, sink);
    hal_display_present();
}

void app_start(void)
{
    uint32_t now_ms = hal_clock_milliseconds();
    key_tracker_init(&app.keys, &KEY_CONFIG);
    frame_stats_reset(&app.frame_stats);
    test_pattern_screen_init(&app.test_pattern, now_ms);
    app.screen = test_pattern_screen_as_screen(&app.test_pattern);
}

void app_step(void)
{
    uint32_t started_us = hal_clock_microseconds();
    uint32_t now_ms = hal_clock_milliseconds();
    read_input();
    key_tracker_update(&app.keys, now_ms);
    dispatch_key_events(now_ms);
    app.screen.update(app.screen.context, now_ms);
    render_frame();
    frame_stats_record(&app.frame_stats, hal_clock_microseconds() - started_us);
}

const FrameStats *app_frame_stats(void)
{
    return &app.frame_stats;
}

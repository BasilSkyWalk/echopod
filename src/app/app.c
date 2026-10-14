#include "app/app.h"

#include <stddef.h>

#include "app_internal.h"
#include "app_screens.h"
#include "core/key_tracker.h"
#include "default_theme.h"
#include "frame_overlay.h"
#include "gfx/strip_renderer.h"
#include "hal/clock.h"
#include "hal/display.h"
#include "hal/input.h"
#include "library_store.h"
#include "player.h"
#include "ui/screen_stack.h"

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
    ScreenStack stack;
    LibraryStore library;
    Player player;
    AppScreens screens;
    AppOptions options;
} AppState;

static AppState app;
static Color strip_pixels[HAL_DISPLAY_WIDTH * STRIP_HEIGHT];

static Screen *top_screen(void)
{
    return screen_stack_top(&app.stack);
}

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
        Screen *screen = top_screen();
        ScreenAction action = screen->handle_key(screen->context, event, now_ms);
        screen_stack_apply(&app.stack, action, now_ms);
    }
}

static void update_screen(uint32_t now_ms)
{
    Screen *screen = top_screen();
    if (screen->update != NULL) {
        screen->update(screen->context, now_ms);
    }
}

static void draw_frame(void *context, Canvas *canvas)
{
    const AppState *state = context;
    Screen *screen = screen_stack_top(&state->stack);
    screen->draw(screen->context, canvas);
    if (state->options.show_frame_time) {
        frame_overlay_draw(canvas, &DEFAULT_THEME, &state->frame_stats);
    }
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
        .background = DEFAULT_THEME.colors.background,
    };
    FrameDrawer drawer = {.draw = draw_frame, .context = &app};
    FrameSink sink = {.write = write_to_display, .context = NULL};
    strip_renderer_render(&renderer, drawer, sink);
    hal_display_present();
}

void app_start(void)
{
    uint32_t now_ms = hal_clock_milliseconds();
    key_tracker_init(&app.keys, &KEY_CONFIG);
    frame_stats_reset(&app.frame_stats);
    app.options = (AppOptions){.show_frame_time = false};
    library_store_open(&app.library);
    player_init(&app.player, &app.library.library);
    player_restore(&app.player);
    AppScreensContext context = {
        .options = &app.options,
        .theme = &DEFAULT_THEME,
        .store = &app.library,
        .player = &app.player,
    };
    Screen *root = app_screens_init(&app.screens, context);
    screen_stack_init(&app.stack, root, now_ms);
}

void app_step(void)
{
    uint32_t started_us = hal_clock_microseconds();
    uint32_t now_ms = hal_clock_milliseconds();
    read_input();
    key_tracker_update(&app.keys, now_ms);
    dispatch_key_events(now_ms);
    player_update(&app.player, now_ms);
    update_screen(now_ms);
    render_frame();
    frame_stats_record(&app.frame_stats, hal_clock_microseconds() - started_us);
}

const FrameStats *app_frame_stats(void)
{
    return &app.frame_stats;
}

const char *app_screen_title(void)
{
    return top_screen()->title;
}

int app_screen_depth(void)
{
    return screen_stack_depth(&app.stack);
}

void app_stop(void)
{
    player_save(&app.player);
}

PlayerStatus app_player_status(void)
{
    return player_status(&app.player);
}

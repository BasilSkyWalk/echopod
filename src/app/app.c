#include "app/app.h"

#include <stddef.h>

#include "app_internal.h"
#include "app_screens.h"
#include "core/key_tracker.h"
#include "core/power_policy.h"
#include "cover_cache.h"
#include "default_theme.h"
#include "favorites_store.h"
#include "frame_overlay.h"
#include "gfx/strip_renderer.h"
#include "hal/clock.h"
#include "hal/display.h"
#include "hal/input.h"
#include "hal/power.h"
#include "library_store.h"
#include "player.h"
#include "settings_store.h"
#include "ui/message_screen.h"
#include "ui/screen_stack.h"
#include "ui/status_bar.h"
#include "ui/toast.h"

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
    SettingsStore settings;
    FavoritesStore favorites;
    CoverCache covers;
    PowerPolicy power;
    Toast toast;
    StatusBarState status_bar;
    AppScreens screens;
    MessageScreen notice;
    AppOptions options;
    bool display_on;
    bool powered_off;
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

static void set_display(bool on)
{
    app.display_on = on;
    hal_display_set_power(on);
}

static void save_settings(void)
{
    settings_store_capture(&app.settings, player_status(&app.player));
    settings_store_save(&app.settings);
}

static void save_session(void)
{
    player_save(&app.player);
    save_settings();
}

static void shut_down(void)
{
    save_session();
    set_display(false);
    app.powered_off = true;
    hal_power_off();
}

static void use_screen_key_behaviors(void)
{
    KeyBehavior volume =
        top_screen()->hold_volume_keys ? KEY_BEHAVIOR_TAP_HOLD_REPEAT : KEY_BEHAVIOR_REPEAT;
    key_tracker_set_behavior(&app.keys, KEY_VOLUME_UP, volume);
    key_tracker_set_behavior(&app.keys, KEY_VOLUME_DOWN, volume);
}

static void dispatch_to_screen(KeyEvent event, uint32_t now_ms)
{
    Screen *screen = top_screen();
    ScreenAction action = screen->handle_key(screen->context, event, now_ms);
    if (screen_stack_apply(&app.stack, action, now_ms)) {
        use_screen_key_behaviors();
    }
}

static void dispatch_key_events(uint32_t now_ms)
{
    KeyEvent event;
    while (!app.powered_off && key_tracker_next_event(&app.keys, &event)) {
        PowerKeyOutcome outcome = power_policy_key(&app.power, event, now_ms);
        if (outcome.shutdown) {
            shut_down();
        } else if (!outcome.consumed) {
            dispatch_to_screen(event, now_ms);
        }
    }
}

static void update_power(uint32_t now_ms)
{
    if (power_policy_update(&app.power, now_ms)) {
        shut_down();
        return;
    }
    if (app.power.display_on == app.display_on) {
        return;
    }
    set_display(app.power.display_on);
    if (!app.display_on) {
        save_settings();
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
    if (!screen->full_screen) {
        status_bar_draw(canvas, &DEFAULT_THEME, state->status_bar);
    }
    toast_draw(&state->toast, canvas, &DEFAULT_THEME);
    if (state->options.show_frame_time) {
        frame_overlay_draw(canvas, &DEFAULT_THEME, &state->frame_stats);
    }
}

static void write_to_display(void *context, Rect area, const Color *pixels)
{
    (void)context;
    hal_display_write(area.x, area.y, area.width, area.height, pixels);
}

static StatusPlayback playback_of(PlayerState state)
{
    switch (state) {
    case PLAYER_PLAYING:
        return STATUS_PLAYBACK_PLAYING;
    case PLAYER_PAUSED:
        return STATUS_PLAYBACK_PAUSED;
    case PLAYER_IDLE:
        return STATUS_PLAYBACK_NONE;
    }
    return STATUS_PLAYBACK_NONE;
}

static StatusBarState current_status_bar(void)
{
    HalBattery battery = hal_power_battery();
    return (StatusBarState){
        .playback = playback_of(player_status(&app.player).state),
        .battery_known = battery.known,
        .battery_percent = battery.percent,
        .charging = battery.charging,
    };
}

static void render(FrameDrawer drawer)
{
    StripRenderer renderer = {
        .strip_pixels = strip_pixels,
        .screen_width = HAL_DISPLAY_WIDTH,
        .screen_height = HAL_DISPLAY_HEIGHT,
        .strip_height = STRIP_HEIGHT,
        .background = DEFAULT_THEME.colors.background,
    };
    FrameSink sink = {.write = write_to_display, .context = NULL};
    strip_renderer_render(&renderer, drawer, sink);
    hal_display_present();
}

static void render_frame(void)
{
    app.status_bar = current_status_bar();
    render((FrameDrawer){.draw = draw_frame, .context = &app});
}

static void draw_notice(void *context, Canvas *canvas)
{
    Screen *screen = context;
    screen->draw(screen->context, canvas);
}

static void start_playback(void)
{
    library_store_open(&app.library);
    player_init(&app.player, &app.library.library);
    settings_store_restore_playback(&app.settings, &app.player, hal_clock_microseconds());
    player_restore(&app.player);
    favorites_store_load(&app.favorites, &app.library.library);
    cover_cache_open(&app.covers, &app.library.library);
}

static void start_hardware(uint32_t now_ms)
{
    power_policy_init(&app.power, settings_screen_timeout_ms(&app.settings.settings), now_ms);
    settings_store_apply_hardware(&app.settings.settings);
    set_display(true);
}

static void open_start_screen(uint32_t now_ms)
{
    Screen *path[START_PATH_CAPACITY];
    int count = app_screens_start_path(&app.screens, app.settings.settings.start_screen, path);
    for (int index = 0; index < count; index++) {
        screen_stack_apply(&app.stack, screen_action_push(path[index]), now_ms);
    }
}

static void start_screens(uint32_t now_ms)
{
    AppScreensContext context = {
        .options = &app.options,
        .theme = &DEFAULT_THEME,
        .store = &app.library,
        .player = &app.player,
        .settings = &app.settings,
        .favorites = &app.favorites,
        .power = &app.power,
        .toast = &app.toast,
        .covers = &app.covers,
    };
    Screen *root = app_screens_init(&app.screens, context);
    screen_stack_init(&app.stack, root, now_ms);
    open_start_screen(now_ms);
    use_screen_key_behaviors();
}

void app_start(void)
{
    uint32_t now_ms = hal_clock_milliseconds();
    key_tracker_init(&app.keys, &KEY_CONFIG);
    frame_stats_reset(&app.frame_stats);
    app.options = (AppOptions){.show_frame_time = false};
    app.toast = (Toast){.visible = false};
    app.powered_off = false;
    settings_store_load(&app.settings);
    start_playback();
    start_hardware(now_ms);
    start_screens(now_ms);
}

void app_step(void)
{
    if (app.powered_off) {
        return;
    }
    uint32_t started_us = hal_clock_microseconds();
    uint32_t now_ms = hal_clock_milliseconds();
    read_input();
    key_tracker_update(&app.keys, now_ms);
    dispatch_key_events(now_ms);
    if (!app.powered_off) {
        player_update(&app.player, now_ms);
        update_power(now_ms);
    }
    if (app.powered_off) {
        return;
    }
    toast_update(&app.toast, now_ms);
    update_screen(now_ms);
    if (app.display_on) {
        render_frame();
    }
    frame_stats_record(&app.frame_stats, hal_clock_microseconds() - started_us);
}

void app_show_notice(const char *title, const char *message)
{
    MessageConfig config = {.title = title, .message = message, .theme = &DEFAULT_THEME};
    Screen *screen = message_screen_init(&app.notice, config);
    if (!app.display_on) {
        set_display(true);
    }
    render((FrameDrawer){.draw = draw_notice, .context = screen});
}

void app_wake(void)
{
    power_policy_wake(&app.power, hal_clock_milliseconds());
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
    if (!app.powered_off) {
        save_session();
    }
}

PlayerStatus app_player_status(void)
{
    return player_status(&app.player);
}

const char *app_toast_text(void)
{
    return app.toast.visible ? app.toast.text : NULL;
}

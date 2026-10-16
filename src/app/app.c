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
#include "gfx/frame_diff.h"
#include "gfx/strip_renderer.h"
#include "hal/audio.h"
#include "hal/clock.h"
#include "hal/display.h"
#include "hal/input.h"
#include "hal/power.h"
#include "library_store.h"
#include "player.h"
#include "settings_store.h"
#include "ui/fade.h"
#include "ui/label.h"
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
    .long_press_ms =
        {
            [KEY_VOLUME_UP] = 1000,
            [KEY_VOLUME_DOWN] = 1000,
            [KEY_POWER] = 2000,
        },
};

typedef struct AppState {
    KeyTracker keys;
    FrameStats frame_stats;
    FrameStats update_stats;
    FrameStats draw_stats;
    FrameStats send_stats;
    uint32_t send_us;
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
    FrameDiff frame_diff;
    Fade splash;
    Fade goodbye;
    bool saying_goodbye;
    uint32_t now_ms;
    AppOptions options;
    bool display_on;
    bool powered_off;
} AppState;

static AppState app;

static const FadeTiming SPLASH_TIMING = {.fade_ms = 600, .hold_ms = 500, .rising = true};
static const FadeTiming GOODBYE_TIMING = {.fade_ms = 600, .hold_ms = 400, .rising = false};
static const char SPLASH_TEXT[] = "Echo Mini";
static const char GOODBYE_TEXT[] = "Goodbye";
static Color strip_pixels[HAL_DISPLAY_WIDTH * STRIP_HEIGHT];
static uint32_t row_hashes[HAL_DISPLAY_HEIGHT];

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

static void use_screen_key_behaviors(void);

static void set_display(bool on)
{
    app.display_on = on;
    hal_display_set_power(on);
    if (screen_stack_depth(&app.stack) > 0) {
        use_screen_key_behaviors();
    }
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

static void finish_shut_down(void)
{
    app.saying_goodbye = false;
    set_display(false);
    app.powered_off = true;
    hal_power_off();
}

static void shut_down(void)
{
    save_session();
    if (!app.display_on) {
        finish_shut_down();
        return;
    }
    fade_start(&app.goodbye, app.now_ms, GOODBYE_TIMING);
    app.saying_goodbye = true;
}

static bool fade_running(void)
{
    return app.saying_goodbye || !fade_finished(&app.splash, app.now_ms);
}

static void drain_key_events(void)
{
    KeyEvent event;
    while (key_tracker_next_event(&app.keys, &event)) {
    }
}

static void use_screen_key_behaviors(void)
{
    bool hold = !app.display_on || top_screen()->hold_volume_keys;
    KeyBehavior volume = hold ? KEY_BEHAVIOR_TAP_HOLD_REPEAT : KEY_BEHAVIOR_REPEAT;
    key_tracker_set_behavior(&app.keys, KEY_VOLUME_UP, volume);
    key_tracker_set_behavior(&app.keys, KEY_VOLUME_DOWN, volume);
}

static bool opens_now_playing(KeyEvent event)
{
    return event.key == KEY_MENU && event.gesture == KEY_GESTURE_PRESS &&
           player_status(&app.player).has_track && top_screen() != &app.screens.now_playing.screen;
}

static void dispatch_to_screen(KeyEvent event, uint32_t now_ms)
{
    Screen *screen = top_screen();
    ScreenAction action = opens_now_playing(event)
                              ? screen_action_push(&app.screens.now_playing.screen)
                              : screen->handle_key(screen->context, event, now_ms);
    if (screen_stack_apply(&app.stack, action, now_ms)) {
        use_screen_key_behaviors();
    }
}

static bool may_show_now_playing(void)
{
    PlayerStatus status = player_status(&app.player);
    Screen *top = top_screen();
    return status.has_track && status.state == PLAYER_PLAYING &&
           top != &app.screens.now_playing.screen && !top->stays_on_wake;
}

static void show_now_playing(uint32_t now_ms)
{
    if (may_show_now_playing() &&
        screen_stack_apply(&app.stack, screen_action_push(&app.screens.now_playing.screen),
                           now_ms)) {
        use_screen_key_behaviors();
    }
}

static void run_background(KeyEvent event)
{
    switch (now_playing_command_for(event, false)) {
    case NOW_PLAYING_TOGGLE_PAUSE:
        player_toggle_pause(&app.player);
        break;
    case NOW_PLAYING_NEXT:
        player_skip(&app.player, 1);
        break;
    case NOW_PLAYING_PREVIOUS:
        player_skip(&app.player, -1);
        break;
    case NOW_PLAYING_VOLUME_UP:
        player_change_volume(&app.player, 1);
        break;
    case NOW_PLAYING_VOLUME_DOWN:
        player_change_volume(&app.player, -1);
        break;
    default:
        break;
    }
}

static void dispatch_key_events(uint32_t now_ms)
{
    KeyEvent event;
    while (!app.powered_off && key_tracker_next_event(&app.keys, &event)) {
        PowerKeyOutcome outcome = power_policy_key(&app.power, event, now_ms);
        if (outcome.shutdown) {
            shut_down();
        } else if (outcome.background) {
            run_background(event);
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
    } else {
        show_now_playing(now_ms);
    }
}

static void update_screen(uint32_t now_ms)
{
    Screen *screen = top_screen();
    if (screen->update != NULL) {
        screen->update(screen->context, now_ms);
    }
}

static void draw_fade(Canvas *canvas, const Fade *fade, const char *text)
{
    const Theme *theme = &DEFAULT_THEME;
    Rect screen = {0, 0, HAL_DISPLAY_WIDTH, HAL_DISPLAY_HEIGHT};
    Color color =
        color_blend(theme->colors.background, theme->colors.text, fade_alpha(fade, app.now_ms));
    TextStyle style = {.font = theme->fonts.title, .color = color};
    label_draw(canvas, style,
               (Label){.area = label_centered_area(style.font, screen, text), .text = text});
}

static bool draw_running_fade(Canvas *canvas)
{
    if (app.saying_goodbye) {
        draw_fade(canvas, &app.goodbye, GOODBYE_TEXT);
        return true;
    }
    if (!fade_finished(&app.splash, app.now_ms)) {
        draw_fade(canvas, &app.splash, SPLASH_TEXT);
        return true;
    }
    return false;
}

static void draw_frame(void *context, Canvas *canvas)
{
    const AppState *state = context;
    if (draw_running_fade(canvas)) {
        return;
    }
    Screen *screen = screen_stack_top(&state->stack);
    screen->draw(screen->context, canvas);
    if (!screen->full_screen) {
        status_bar_draw(canvas, &DEFAULT_THEME, state->status_bar);
    }
    toast_draw(&state->toast, canvas, &DEFAULT_THEME);
    if (state->options.show_frame_time) {
        FrameBreakdown breakdown = {
            .total_us = frame_stats_average_us(&state->frame_stats),
            .update_us = frame_stats_average_us(&state->update_stats),
            .draw_us = frame_stats_average_us(&state->draw_stats),
            .send_us = frame_stats_average_us(&state->send_stats),
        };
        frame_overlay_draw(canvas, &DEFAULT_THEME, breakdown, hal_audio_stats());
    }
}

static void write_to_display(void *context, Rect area, const Color *pixels)
{
    (void)context;
    Rect changed;
    hal_audio_service();
    if (!frame_diff_changed(&app.frame_diff, area, pixels, &changed)) {
        return;
    }
    const Color *first_row = pixels + (size_t)(changed.y - area.y) * (size_t)area.width;
    uint32_t started_us = hal_clock_microseconds();
    hal_display_write(changed.x, changed.y, changed.width, changed.height, first_row);
    app.send_us += hal_clock_microseconds() - started_us;
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

static void render(FrameDrawer drawer, bool drawer_paints_everything)
{
    StripRenderer renderer = {
        .strip_pixels = strip_pixels,
        .screen_width = HAL_DISPLAY_WIDTH,
        .screen_height = HAL_DISPLAY_HEIGHT,
        .strip_height = STRIP_HEIGHT,
        .background = DEFAULT_THEME.colors.background,
        .drawer_paints_everything = drawer_paints_everything,
    };
    FrameSink sink = {.write = write_to_display, .context = NULL};
    strip_renderer_render(&renderer, drawer, sink);
    hal_display_present();
}

static void render_frame(void)
{
    uint32_t started_us = hal_clock_microseconds();
    app.send_us = 0;
    app.status_bar = current_status_bar();
    bool screen_paints = top_screen()->paints_every_pixel && !fade_running();
    render((FrameDrawer){.draw = draw_frame, .context = &app}, screen_paints);
    uint32_t render_us = hal_clock_microseconds() - started_us;
    frame_stats_record(&app.send_stats, app.send_us);
    frame_stats_record(&app.draw_stats, render_us - app.send_us);
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
    frame_diff_init(&app.frame_diff, row_hashes, HAL_DISPLAY_HEIGHT);
    key_tracker_init(&app.keys, &KEY_CONFIG);
    frame_stats_reset(&app.frame_stats);
    frame_stats_reset(&app.update_stats);
    frame_stats_reset(&app.draw_stats);
    frame_stats_reset(&app.send_stats);
    app.options = (AppOptions){.show_frame_time = false};
    app.toast = (Toast){.visible = false};
    app.powered_off = false;
    app.saying_goodbye = false;
    app.splash = (Fade){.active = false};
    app.now_ms = now_ms;
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
    app.now_ms = now_ms;
    hal_audio_service();
    read_input();
    key_tracker_update(&app.keys, now_ms);
    if (fade_running()) {
        drain_key_events();
    } else {
        dispatch_key_events(now_ms);
    }
    if (app.saying_goodbye && fade_finished(&app.goodbye, now_ms)) {
        finish_shut_down();
    }
    if (!app.powered_off) {
        player_update(&app.player, now_ms);
        update_power(now_ms);
    }
    if (app.powered_off) {
        return;
    }
    toast_update(&app.toast, now_ms);
    update_screen(now_ms);
    frame_stats_record(&app.update_stats, hal_clock_microseconds() - started_us);
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
    render((FrameDrawer){.draw = draw_notice, .context = screen}, false);
}

void app_show_splash(void)
{
    app.now_ms = hal_clock_milliseconds();
    fade_start(&app.splash, app.now_ms, SPLASH_TIMING);
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

const char *app_diagnostics_value(int index)
{
    const MenuItem *item = &app.screens.diagnostics_items[index];
    return item->value != NULL ? item->value(item->context) : NULL;
}

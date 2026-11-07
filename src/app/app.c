#include "app/app.h"

#include <stddef.h>

#include "app_internal.h"
#include "app_screens.h"
#include "bluetooth_link.h"
#include "card_files.h"
#include "clock_store.h"
#include "core/deadline.h"
#include "core/i18n.h"
#include "core/key_tracker.h"
#include "core/low_battery.h"
#include "core/power_policy.h"
#include "core/string_builder.h"
#include "cover_cache.h"
#include "default_theme.h"
#include "draw_profile.h"
#include "equalizer_store.h"
#include "favorites_store.h"
#include "frame_overlay.h"
#include "gfx/frame_diff.h"
#include "gfx/strip_renderer.h"
#include "hal/audio.h"
#include "hal/bluetooth.h"
#include "hal/clock.h"
#include "hal/display.h"
#include "hal/input.h"
#include "hal/language.h"
#include "hal/power.h"
#include "hal/system.h"
#include "library_rescan.h"
#include "library_store.h"
#include "listen_history.h"
#include "music_check.h"
#include "player.h"
#include "screens/charge_screen.h"
#include "scrobbler_log.h"
#include "settings/rocker.h"
#include "settings_store.h"
#include "ui/choice_screen.h"
#include "ui/fade.h"
#include "ui/header.h"
#include "ui/label.h"
#include "ui/marquee.h"
#include "ui/message_screen.h"
#include "ui/screen_stack.h"
#include "ui/slide.h"
#include "ui/status_bar.h"
#include "ui/toast.h"

enum {
    STRIP_HEIGHT = 34,
    NOTICE_STRIP_HEIGHT = 6,
    SEND_ROWS = 4,
    RESCAN_PROGRESS_MS = 250,
    BATTERY_CHECK_MS = 250,
    CHARGE_LIT_MS = 5000,
    IDLE_RENDER_MS = 100,
    NUMBER_CAPACITY = 12,
    PROGRESS_TEXT_CAPACITY = 64,
    MESSAGE_TEXT_CAPACITY = 64
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

static const KeyConfig DAC_KEY_CONFIG = {
    .timing =
        {
            .long_press_ms = 600,
            .repeat_delay_ms = 400,
            .repeat_interval_ms = 120,
        },
    .behaviors =
        {
            [KEY_VOLUME_UP] = KEY_BEHAVIOR_TAP_HOLD_REPEAT,
            [KEY_VOLUME_DOWN] = KEY_BEHAVIOR_TAP_HOLD_REPEAT,
            [KEY_PLAY] = KEY_BEHAVIOR_TAP_OR_HOLD,
            [KEY_MENU] = KEY_BEHAVIOR_TAP_OR_HOLD,
            [KEY_BACK] = KEY_BEHAVIOR_TAP_OR_HOLD,
            [KEY_POWER] = KEY_BEHAVIOR_TAP_OR_HOLD,
        },
    .long_press_ms =
        {
            [KEY_VOLUME_UP] = 1000,
            [KEY_VOLUME_DOWN] = 1000,
            [KEY_BACK] = 1000,
            [KEY_POWER] = 2000,
        },
};

typedef struct AppState {
    KeyTracker keys;
    bool rocker_swapped[KEY_COUNT];
    uint32_t dac_volume_until_ms;
    Theme theme;
    Theme stage_theme;
    int shown_theme;
    FrameStats frame_stats;
    FrameStats update_stats;
    FrameStats draw_stats;
    FrameStats send_stats;
    uint32_t send_us;
    ScreenStack stack;
    LibraryStore library;
    Player player;
    SettingsStore settings;
    EqualizerStore equalizer;
    FavoritesStore favorites;
    SpokenStore spoken;
    ListenHistory history;
    ClockStore clock;
    bool usb_written;
    CoverCache covers;
    PowerPolicy power;
    LowBattery low_battery;
    uint32_t battery_check_ms;
    bool usb_power;
    bool rescan_requested;
    bool music_check_requested;
    bool clock_question;
    bool open_date_time;
    bool light_after_frame;
    bool frame_changed;
    bool frame_stable;
    bool key_activity;
    bool idle;
    uint32_t idle_render_ms;
    const char *goodbye_text;
    Toast toast;
    BluetoothLink bluetooth;
    uint32_t shown_refusals;
    StatusBarState status_bar;
    AppScreens screens;
    MessageScreen notice;
    ChoiceScreen prompt;
    ChoiceScreen clock_prompt;
    int prompt_default;
    int prompt_result;
    FrameDiff frame_diff;
    Fade splash;
    Fade goodbye;
    Slide slide;
    Screen *leaving;
    bool saying_goodbye;
    uint32_t now_ms;
    AppOptions options;
    bool display_on;
    bool powered_off;
    bool charging;
    uint32_t charge_lit_until_ms;
    HalBattery charge_battery;
} AppState;

static AppState app;

static const FadeTiming SPLASH_TIMING = {.fade_ms = 300, .hold_ms = 200, .rising = true};
static const uint32_t SCREEN_ON_CLOCK_MHZ = 200;
static const uint32_t IDLE_CLOCK_MHZ = 96;
static const FadeTiming GOODBYE_TIMING = {.fade_ms = 600, .hold_ms = 400, .rising = false};
static const char SPLASH_TEXT[] = "EchoPod";
static const char GOODBYE_TEXT[] = "Goodbye";
static Color strip_pixels[HAL_DISPLAY_WIDTH * STRIP_HEIGHT];
static uint32_t row_hashes[HAL_DISPLAY_LONG_SIDE];

static Screen *top_screen(void)
{
    return screen_stack_top(&app.stack);
}

static const Theme *current_theme(void)
{
    return app.theme.metrics.screen_width > 0 ? &app.theme : &DEFAULT_THEME;
}

static Rect whole_screen(void)
{
    const ThemeMetrics *metrics = &current_theme()->metrics;
    return (Rect){0, 0, metrics->screen_width, metrics->screen_height};
}

static HalDisplayRotation rotation_for(Orientation orientation)
{
    switch (orientation) {
    case ORIENTATION_PORTRAIT:
        return HAL_DISPLAY_PORTRAIT;
    case ORIENTATION_PORTRAIT_FLIPPED:
        return HAL_DISPLAY_PORTRAIT_FLIPPED;
    case ORIENTATION_LANDSCAPE:
        return HAL_DISPLAY_LANDSCAPE;
    }
    return HAL_DISPLAY_LANDSCAPE;
}

static int theme_key(const Settings *settings)
{
    return (int)settings->orientation | settings->options[OPTION_ACCENT] << 2 |
           settings->options[OPTION_HIGHLIGHT] << 5 | settings->options[OPTION_THEME] << 6 |
           settings->options[OPTION_STATUS_BAR] << 7;
}

static void apply_orientation(void)
{
    const Settings *settings = &app.settings.settings;
    Orientation orientation = settings->orientation;
    app.shown_theme = theme_key(settings);
    bool portrait = orientation != ORIENTATION_LANDSCAPE;
    int accent = settings->options[OPTION_ACCENT];
    bool flat = settings->options[OPTION_HIGHLIGHT] != 0;
    app.theme = default_theme_styled(portrait, accent, flat,
                                     settings->options[OPTION_THEME] == THEME_LIGHT);
    app.stage_theme = default_theme_styled(portrait, accent, flat, false);
    if (settings->options[OPTION_STATUS_BAR] == STATUS_BAR_LARGE) {
        default_theme_enlarge_status(&app.theme);
        default_theme_enlarge_status(&app.stage_theme);
    }
    hal_display_set_rotation(rotation_for(orientation));
    frame_diff_init(&app.frame_diff, row_hashes, app.theme.metrics.screen_height);
}

static void follow_orientation(void)
{
    if (theme_key(&app.settings.settings) != app.shown_theme) {
        apply_orientation();
    }
    app.power.menu_is_media = app.settings.settings.options[OPTION_MENU_KEY] == MENU_KEY_NEXT;
}

static Key oriented_key(HalKeyTransition transition)
{
    if (transition.pressed) {
        const Settings *settings = &app.settings.settings;
        app.rocker_swapped[transition.key] = rocker_swapped_in(settings->orientation) !=
                                             (settings->options[OPTION_VOLUME_KEYS] != 0);
    }
    return rocker_key(transition.key, app.rocker_swapped[transition.key]);
}

static void read_input(void)
{
    HalKeyTransition transition;
    while (hal_input_next_transition(&transition)) {
        Key key = oriented_key(transition);
        if (transition.pressed) {
            key_tracker_key_down(&app.keys, key, transition.time_ms);
        } else {
            key_tracker_key_up(&app.keys, key, transition.time_ms);
        }
    }
}

static void use_screen_key_behaviors(void);

static void use_wanted_clock(void)
{
    hal_audio_allow_slow_clock(!app.display_on);
    if (!hal_audio_status().idle) {
        return;
    }
    uint32_t wanted = app.display_on ? SCREEN_ON_CLOCK_MHZ : IDLE_CLOCK_MHZ;
    if (hal_system_clock_mhz() != wanted) {
        hal_system_set_clock_mhz(wanted);
    }
}

static void light_after_first_frame(void)
{
    if (app.light_after_frame) {
        app.light_after_frame = false;
        hal_display_set_power(true);
    }
}

static void set_display(bool on)
{
    app.display_on = on;
    app.light_after_frame = on;
    app.frame_stable = false;
    if (on) {
        frame_diff_forget(&app.frame_diff);
    } else {
        hal_display_set_power(false);
    }
    use_wanted_clock();
    if (screen_stack_depth(&app.stack) > 0) {
        use_screen_key_behaviors();
    }
}

static void save_settings(void)
{
    settings_store_capture(&app.settings, player_status(&app.player));
    settings_store_save(&app.settings);
    equalizer_store_save(&app.equalizer);
}

static bool history_on(void)
{
    return app.settings.settings.options[OPTION_LISTEN_HISTORY] == LISTEN_HISTORY_ON;
}

static void follow_history(const PlayerStatus *status, uint32_t now_ms)
{
    if (history_on()) {
        listen_history_follow(&app.history, &app.library.library, status, now_ms);
    } else {
        listen_history_close(&app.history, now_ms);
    }
}

static void save_session(void)
{
    listen_history_close(&app.history, hal_clock_milliseconds());
    spoken_store_flush(&app.spoken, player_status(&app.player), hal_clock_milliseconds());
    player_save(&app.player);
    save_settings();
}

static void power_off_now(void)
{
    set_display(false);
    app.powered_off = true;
    hal_power_off();
}

static void light_charge_screen(uint32_t now_ms)
{
    app.charge_lit_until_ms = now_ms + CHARGE_LIT_MS;
    if (!app.display_on) {
        set_display(true);
    }
}

static void start_charging(uint32_t now_ms)
{
    if (player_status(&app.player).state == PLAYER_PLAYING) {
        player_toggle_pause(&app.player);
    }
    player_release_output(&app.player);
    bluetooth_link_sleep(&app.bluetooth);
    app.charging = true;
    app.charge_battery = hal_power_battery();
    light_charge_screen(now_ms);
}

static void stop_charging(uint32_t now_ms)
{
    app.charging = false;
    bluetooth_link_start(&app.bluetooth, &app.toast, now_ms);
    power_policy_wake(&app.power, now_ms);
    if (!app.display_on) {
        set_display(true);
    }
    app_show_splash();
}

static void finish_shut_down(void)
{
    app.saying_goodbye = false;
    if (hal_power_battery().usb_power) {
        start_charging(app.now_ms);
        return;
    }
    power_off_now();
}

static void shut_down(const char *text, bool shown)
{
    app.goodbye_text = text;
    save_session();
    if (shown && !app.display_on) {
        power_policy_wake(&app.power, app.now_ms);
        set_display(true);
    }
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
    bool at_once = app.display_on && top_screen()->select_on_press;
    key_tracker_set_behavior(&app.keys, KEY_PLAY,
                             at_once ? KEY_BEHAVIOR_PRESS : KEY_BEHAVIOR_TAP_OR_HOLD);
}

static bool opens_now_playing(KeyEvent event)
{
    return event.key == KEY_MENU && event.gesture == KEY_GESTURE_PRESS &&
           player_status(&app.player).has_track && top_screen() != &app.screens.now_playing.screen;
}

static bool settings_open(void)
{
    for (int index = 0; index < app.stack.depth; index++) {
        if (app.stack.screens[index] == &app.screens.settings.menu.screen) {
            return true;
        }
    }
    return false;
}

static bool opens_settings(KeyEvent event)
{
    return event.key == KEY_MENU && event.gesture == KEY_GESTURE_LONG_PRESS &&
           top_screen() != &app.screens.now_playing.screen && !settings_open();
}

static ScreenAction now_playing_action(void)
{
    return top_screen() == &app.screens.up_next.screen
               ? screen_action_pop()
               : screen_action_push(&app.screens.now_playing.screen);
}

static void start_slide(Screen *leaving, int depth_before, uint32_t now_ms)
{
    if (top_screen() == leaving) {
        return;
    }
    bool deeper = screen_stack_depth(&app.stack) > depth_before;
    app.leaving = leaving;
    slide_start(&app.slide, now_ms, deeper ? SLIDE_FORWARD : SLIDE_BACK,
                settings_animation_ms(app.settings.settings.options, SLIDE_DURATION_MS));
}

static bool slide_running(void)
{
    return app.slide.active && !slide_finished(&app.slide, app.now_ms);
}

static void dispatch_to_screen(KeyEvent event, uint32_t now_ms)
{
    Screen *screen = top_screen();
    int depth_before = screen_stack_depth(&app.stack);
    ScreenAction action = opens_now_playing(event) ? now_playing_action()
                          : opens_settings(event)
                              ? screen_action_push(&app.screens.settings.menu.screen)
                              : screen->handle_key(screen->context, event, now_ms);
    if (screen_stack_apply(&app.stack, action, now_ms)) {
        use_screen_key_behaviors();
        start_slide(screen, depth_before, now_ms);
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
    NowPlayingControls controls = now_playing_controls_of(app.settings.settings.options);
    NowPlayingCommand command = now_playing_command_with(event, false, controls);
    switch (command) {
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
    case NOW_PLAYING_SEEK_FORWARD:
    case NOW_PLAYING_SEEK_BACK:
        player_jump(&app.player, command == NOW_PLAYING_SEEK_FORWARD ? NOW_PLAYING_SCRUB_STEP_MS
                                                                     : -NOW_PLAYING_SCRUB_STEP_MS);
        break;
    default:
        break;
    }
}

static bool ignored_power_tap(KeyEvent event)
{
    return app.options.ignore_power_taps && event.key == KEY_POWER &&
           event.gesture == KEY_GESTURE_PRESS;
}

static void dispatch_key_events(uint32_t now_ms)
{
    player_sync(&app.player);
    KeyEvent event;
    while (!app.powered_off && key_tracker_next_event(&app.keys, &event)) {
        app.key_activity = true;
        if (ignored_power_tap(event)) {
            continue;
        }
        PowerKeyOutcome outcome = power_policy_key(&app.power, event, now_ms);
        if (outcome.shutdown) {
            shut_down(GOODBYE_TEXT, true);
        } else if (outcome.background) {
            run_background(event);
        } else if (!outcome.consumed) {
            dispatch_to_screen(event, now_ms);
        }
    }
}

static bool battery_empty(uint32_t now_ms)
{
    if (!deadline_reached(now_ms, app.battery_check_ms)) {
        return false;
    }
    app.battery_check_ms = now_ms + BATTERY_CHECK_MS;
    HalBattery battery = hal_power_battery();
    app.usb_power = battery.usb_power;
    LowBatteryReading reading = {
        .known = battery.known, .millivolts = battery.millivolts, .usb_power = battery.usb_power};
    return low_battery_update(&app.low_battery, reading, now_ms);
}

static void keep_awake_while_busy(uint32_t now_ms)
{
    if (app.usb_power || player_status(&app.player).state == PLAYER_PLAYING) {
        power_policy_stay_awake(&app.power, now_ms);
    }
}

static void update_power(uint32_t now_ms)
{
    if (!app.saying_goodbye && battery_empty(now_ms)) {
        shut_down(i18n_text(STR_BOOT_BATTERY_LOW), false);
        return;
    }
    keep_awake_while_busy(now_ms);
    if (power_policy_update(&app.power, now_ms)) {
        shut_down(GOODBYE_TEXT, false);
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
    const Theme *theme = current_theme();
    Rect screen = whole_screen();
    Color color =
        color_blend(theme->colors.background, theme->colors.text, fade_alpha(fade, app.now_ms));
    TextStyle style = {.font = theme->fonts.title, .color = color};
    label_draw(canvas, style,
               (Label){.area = label_centered_area(style.font, screen, text), .text = text});
}

static bool draw_running_fade(Canvas *canvas)
{
    if (app.saying_goodbye) {
        draw_fade(canvas, &app.goodbye, app.goodbye_text);
        return true;
    }
    if (!fade_finished(&app.splash, app.now_ms)) {
        draw_fade(canvas, &app.splash, SPLASH_TEXT);
        return true;
    }
    return false;
}

static void draw_shifted(Screen *screen, Canvas *canvas, int x, Rect area)
{
    Canvas shifted = canvas_shifted(canvas, x, area);
    if (!rect_is_empty(shifted.clip)) {
        screen->draw(screen->context, &shifted);
    }
}

static void draw_slide(Canvas *canvas, Screen *arriving)
{
    const Theme *theme = current_theme();
    Screen *leaving = app.leaving;
    bool whole = arriving->full_screen || leaving->full_screen;
    Rect area = whole ? whole_screen() : theme_content_area(theme);
    int width = theme->metrics.screen_width;
    draw_shifted(leaving, canvas, slide_leaving_x(&app.slide, app.now_ms, width), area);
    draw_shifted(arriving, canvas, slide_arriving_x(&app.slide, app.now_ms, width), area);
    if (!whole) {
        bool arrived = slide_shows_arriving_title(&app.slide, app.now_ms);
        header_draw(canvas, theme, arrived ? arriving->title : leaving->title);
    }
}

static void draw_frame(void *context, Canvas *canvas)
{
    const AppState *state = context;
    if (draw_running_fade(canvas)) {
        return;
    }
    Screen *screen = screen_stack_top(&state->stack);
    if (slide_running()) {
        draw_slide(canvas, screen);
    } else {
        screen->draw(screen->context, canvas);
    }
    if (!screen->full_screen) {
        status_bar_draw(canvas, current_theme(), state->status_bar);
    }
    toast_draw(&state->toast, canvas, current_theme());
    if (state->options.show_frame_time) {
        FrameBreakdown breakdown = {
            .total_us = frame_stats_average_us(&state->frame_stats),
            .update_us = frame_stats_average_us(&state->update_stats),
            .draw_us = frame_stats_average_us(&state->draw_stats),
            .send_us = frame_stats_average_us(&state->send_stats),
        };
        frame_overlay_draw(canvas, current_theme(), breakdown, hal_audio_stats());
    }
}

static void write_to_display(void *context, Rect area, const Color *pixels)
{
    (void)context;
    Rect changed;
    uint32_t audio_started_us = hal_clock_microseconds();
    hal_audio_service();
    hal_bluetooth_service();
    uint32_t hash_started_us = hal_clock_microseconds();
    draw_profile_add(DRAW_PART_AUDIO, hash_started_us - audio_started_us);
    bool any_changed = frame_diff_changed(&app.frame_diff, area, pixels, &changed);
    app.frame_changed |= any_changed;
    draw_profile_add(DRAW_PART_HASH, hal_clock_microseconds() - hash_started_us);
    if (!any_changed) {
        return;
    }
    const Color *first_row = pixels + (size_t)(changed.y - area.y) * (size_t)area.width;
    for (int sent = 0; sent < changed.height; sent += SEND_ROWS) {
        int rows = changed.height - sent < SEND_ROWS ? changed.height - sent : SEND_ROWS;
        uint32_t started_us = hal_clock_microseconds();
        hal_display_write(changed.x, changed.y + sent, changed.width, rows,
                          first_row + (size_t)sent * (size_t)area.width);
        app.send_us += hal_clock_microseconds() - started_us;
        hal_audio_service();
    }
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

static StatusBluetooth bluetooth_of(const BluetoothLink *link)
{
    if (!link->saved.enabled || link->dozing) {
        return STATUS_BLUETOOTH_OFF;
    }
    return link->connected ? STATUS_BLUETOOTH_CONNECTED : STATUS_BLUETOOTH_ON;
}

static const char *status_clock_text(void)
{
    static char text[WALL_CLOCK_TIME_CAPACITY];
    WallCivil civil;
    if (app.settings.settings.options[OPTION_CLOCK] != CLOCK_ON ||
        !clock_store_now(&app.clock, app.now_ms, &civil)) {
        return NULL;
    }
    wall_clock_time_text(&civil, text, sizeof text);
    return text;
}

static StatusBarState current_status_bar(void)
{
    HalBattery battery = hal_power_battery();
    return (StatusBarState){
        .playback = playback_of(player_status(&app.player).state),
        .bluetooth = bluetooth_of(&app.bluetooth),
        .battery_known = battery.known,
        .battery_percent = battery.percent,
        .charging = battery.charging,
        .hide_percent = app.settings.settings.options[OPTION_BATTERY] != 0,
        .clock = status_clock_text(),
    };
}

static void render(FrameDrawer drawer, bool drawer_paints_everything, int strip_height)
{
    StripRenderer renderer = {
        .strip_pixels = strip_pixels,
        .screen_width = current_theme()->metrics.screen_width,
        .screen_height = current_theme()->metrics.screen_height,
        .strip_height = strip_height,
        .background = current_theme()->colors.background,
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
    app.frame_changed = false;
    app.status_bar = current_status_bar();
    bool screen_paints = top_screen()->paints_every_pixel && !fade_running() && !slide_running();
    marquee_frame_start();
    render((FrameDrawer){.draw = draw_frame, .context = &app}, screen_paints, STRIP_HEIGHT);
    uint32_t render_us = hal_clock_microseconds() - started_us;
    frame_stats_record(&app.send_stats, app.send_us);
    frame_stats_record(&app.draw_stats, render_us - app.send_us);
    draw_profile_finish_frame();
    app.frame_stable = !app.frame_changed;
    app.idle_render_ms = hal_clock_milliseconds() + IDLE_RENDER_MS;
    uint32_t marquee_wait_ms = marquee_frame_wait_ms();
    if (marquee_wait_ms > 0 &&
        !deadline_reached(app.now_ms + marquee_wait_ms, app.idle_render_ms)) {
        app.idle_render_ms = app.now_ms + marquee_wait_ms;
    }
}

static bool frame_needed(uint32_t now_ms)
{
    bool busy = app.key_activity || app.options.show_frame_time || app.light_after_frame ||
                fade_running() || slide_running();
    return busy || !app.frame_stable || deadline_reached(now_ms, app.idle_render_ms);
}

static void draw_notice(void *context, Canvas *canvas)
{
    Screen *screen = context;
    screen->draw(screen->context, canvas);
}

static void start_playback(void)
{
    cover_cache_close(&app.covers);
    library_store_open(&app.library);
    player_init(&app.player, &app.library.library);
    settings_store_restore_playback(&app.settings, &app.player, hal_clock_microseconds());
    player_restore(&app.player);
    favorites_store_load(&app.favorites, &app.library.library);
    spoken_store_load(&app.spoken, &app.library.library);
    player_set_resume(&app.player, spoken_store_resume(&app.spoken));
    cover_cache_open(&app.covers, &app.library.library);
}

static void start_hardware(uint32_t now_ms)
{
    power_policy_init(&app.power, settings_screen_timeout_ms(&app.settings.settings), now_ms);
    power_policy_set_auto_off(&app.power, settings_auto_off_ms(&app.settings.settings), now_ms);
    settings_store_apply_hardware(&app.settings.settings);
    equalizer_store_apply(&app.equalizer);
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
        .theme = &app.theme,
        .stage_theme = &app.stage_theme,
        .store = &app.library,
        .player = &app.player,
        .spoken = &app.spoken,
        .settings = &app.settings,
        .favorites = &app.favorites,
        .power = &app.power,
        .toast = &app.toast,
        .covers = &app.covers,
        .rescan_requested = &app.rescan_requested,
        .bluetooth = &app.bluetooth,
        .equalizer = &app.equalizer,
        .clock = &app.clock,
    };
    Screen *root = app_screens_init(&app.screens, context);
    screen_stack_init(&app.stack, root, now_ms);
    open_start_screen(now_ms);
    use_screen_key_behaviors();
}

static void report_card_files(CardFilesReport files, uint32_t now_ms)
{
    if (files.card_full) {
        toast_show(&app.toast, i18n_text(STR_BOOT_CARD_FULL), now_ms);
    }
    if (files.library_created) {
        app.rescan_requested = true;
    } else if (music_check_firmware_is_new()) {
        app.music_check_requested = true;
        app.clock_question = true;
    }
}

static void show_card_setup(void)
{
    app_show_notice("EchoPod", i18n_text(STR_BOOT_CARD_SETUP));
}

static void apply_language(Language language)
{
    const uint8_t *pack;
    uint32_t size;
    if (!hal_language_pack((uint32_t)language, &pack, &size) || !i18n_attach(pack, size)) {
        i18n_detach();
    }
}

void app_start(void)
{
    uint32_t now_ms = hal_clock_milliseconds();
    apply_language(LANGUAGE_ENGLISH);
    key_tracker_init(&app.keys, &KEY_CONFIG);
    frame_stats_reset(&app.frame_stats);
    frame_stats_reset(&app.update_stats);
    frame_stats_reset(&app.draw_stats);
    frame_stats_reset(&app.send_stats);
    app.options = (AppOptions){.show_frame_time = false, .ignore_power_taps = false};
    app.toast = (Toast){.visible = false};
    app.powered_off = false;
    app.charging = false;
    app.frame_stable = false;
    app.key_activity = false;
    app.idle = false;
    app.saying_goodbye = false;
    app.goodbye_text = GOODBYE_TEXT;
    low_battery_init(&app.low_battery);
    app.battery_check_ms = now_ms;
    app.splash = (Fade){.active = false};
    app.now_ms = now_ms;
    CardFilesReport files = card_files_prepare(show_card_setup);
    settings_store_load(&app.settings);
    hal_power_confirm_start(app.settings.settings.options[OPTION_POWER_ON] == POWER_ON_HOLD);
    clock_store_start(&app.clock, &app.settings.settings, now_ms);
    app.history.clock = &app.clock.clock;
    apply_language(app.settings.settings.language);
    apply_orientation();
    equalizer_store_load(&app.equalizer);
    start_playback();
    start_hardware(now_ms);
    bluetooth_link_start(&app.bluetooth, &app.toast, now_ms);
    start_screens(now_ms);
    report_card_files(files, now_ms);
    if (hal_power_take_usb_start()) {
        start_charging(now_ms);
    }
}

static void number_text(char *text, size_t capacity, uint32_t value)
{
    StringBuilder builder = string_builder_make(text, capacity);
    string_builder_append_unsigned(&builder, value);
}

static void show_rescan_progress(uint32_t files, uint32_t tracks)
{
    static uint32_t shown_ms;
    uint32_t now_ms = hal_clock_milliseconds();
    if (files > 1 && now_ms - shown_ms < RESCAN_PROGRESS_MS) {
        return;
    }
    shown_ms = now_ms;
    static char text[PROGRESS_TEXT_CAPACITY];
    char count[NUMBER_CAPACITY];
    number_text(count, sizeof count, tracks);
    const char *arguments[] = {count};
    StringBuilder builder = string_builder_make(text, sizeof text);
    i18n_format(&builder, STR_RESCAN_TRACKS_SO_FAR, arguments, 1);
    app_show_notice(i18n_text(STR_RESCAN_TITLE), text);
}

static void show_cover_progress(uint32_t done, uint32_t total)
{
    static uint32_t shown_ms;
    uint32_t now_ms = hal_clock_milliseconds();
    if (done > 0 && done < total && now_ms - shown_ms < RESCAN_PROGRESS_MS) {
        return;
    }
    shown_ms = now_ms;
    static char text[PROGRESS_TEXT_CAPACITY];
    char finished[NUMBER_CAPACITY];
    char albums[NUMBER_CAPACITY];
    number_text(finished, sizeof finished, done);
    number_text(albums, sizeof albums, total);
    const char *arguments[] = {finished, albums};
    StringBuilder builder = string_builder_make(text, sizeof text);
    i18n_format(&builder, STR_RESCAN_ALBUMS_PROGRESS, arguments, 2);
    app_show_notice(i18n_text(STR_RESCAN_COVERS_TITLE), text);
}

static void explain_refusals(uint32_t now_ms)
{
    static char text[MESSAGE_TEXT_CAPACITY];
    uint32_t count = app.player.refusal_count;
    bool fresh = count > app.shown_refusals;
    app.shown_refusals = count;
    if (fresh) {
        player_refusal_text(app.player.refusal, text, sizeof text);
        toast_show(&app.toast, text, now_ms);
    }
}

static void run_rescan(void)
{
    listen_history_close(&app.history, hal_clock_milliseconds());
    app_show_notice(i18n_text(STR_RESCAN_TITLE), i18n_text(STR_RESCAN_STARTING));
    RescanProgress progress = {show_rescan_progress, show_cover_progress};
    RescanOutcome outcome = library_rescan_run(progress);
    static char message[MESSAGE_TEXT_CAPACITY];
    const char *text = library_rescan_message(outcome, message, sizeof message);
    if (outcome.result == SCAN_OK) {
        music_check_remember(music_check_fingerprint());
        app_start();
    }
    app_wake();
    toast_show(&app.toast, text, hal_clock_milliseconds());
}

void app_release_audio(void)
{
    if (history_on()) {
        listen_history_note_computer(&app.history, hal_clock_milliseconds());
        (void)scrobbler_log_write(&app.history, hal_system_version());
    }
    spoken_store_flush(&app.spoken, player_status(&app.player), hal_clock_milliseconds());
    player_release_output(&app.player);
}

void app_remember_dac_volume(uint8_t level)
{
    app.settings.settings.dac_volume = level;
    settings_store_save(&app.settings);
}

void app_dac_keys_start(void)
{
    key_tracker_init(&app.keys, &DAC_KEY_CONFIG);
    app.dac_volume_until_ms = hal_clock_milliseconds();
}

static AppDacKey dac_key_of(NowPlayingCommand command)
{
    switch (command) {
    case NOW_PLAYING_VOLUME_UP:
        return APP_DAC_KEY_VOLUME_UP;
    case NOW_PLAYING_VOLUME_DOWN:
        return APP_DAC_KEY_VOLUME_DOWN;
    case NOW_PLAYING_TOGGLE_PAUSE:
        return APP_DAC_KEY_PLAY_PAUSE;
    case NOW_PLAYING_NEXT:
        return APP_DAC_KEY_NEXT;
    case NOW_PLAYING_PREVIOUS:
        return APP_DAC_KEY_PREVIOUS;
    case NOW_PLAYING_HOME:
        return APP_DAC_KEY_LEAVE;
    default:
        return APP_DAC_KEY_NONE;
    }
}

static AppDacKey dac_key_for(KeyEvent event, uint32_t now_ms)
{
    NowPlayingControls controls = now_playing_controls_of(app.settings.settings.options);
    controls.volume_shown = !deadline_reached(now_ms, app.dac_volume_until_ms);
    AppDacKey key = dac_key_of(now_playing_command_with(event, false, controls));
    if (key == APP_DAC_KEY_VOLUME_UP || key == APP_DAC_KEY_VOLUME_DOWN) {
        app.dac_volume_until_ms = now_ms + NOW_PLAYING_VOLUME_HOLD_MS;
    }
    return key;
}

AppDacKey app_dac_next_key(void)
{
    uint32_t now_ms = hal_clock_milliseconds();
    read_input();
    key_tracker_update(&app.keys, now_ms);
    KeyEvent event;
    while (key_tracker_next_event(&app.keys, &event)) {
        AppDacKey key = dac_key_for(event, now_ms);
        if (key != APP_DAC_KEY_NONE) {
            return key;
        }
    }
    return APP_DAC_KEY_NONE;
}

static int prompt_current(const void *context)
{
    const AppState *state = context;
    return state->prompt_default;
}

static void prompt_choose(void *context, int index)
{
    AppState *state = context;
    state->prompt_result = index;
}

void app_choice_show(const char *title, const StringId *options, int count, int current)
{
    app.prompt_default = current;
    app.prompt_result = -1;
    ChoiceConfig config = {.title = title,
                           .options = NULL,
                           .option_ids = options,
                           .option_count = count,
                           .current = prompt_current,
                           .choose = prompt_choose,
                           .context = &app,
                           .theme = current_theme()};
    Screen *screen = choice_screen_init(&app.prompt, config);
    app.now_ms = hal_clock_milliseconds();
    screen->enter(screen->context, app.now_ms);
    key_tracker_init(&app.keys, &KEY_CONFIG);
    key_tracker_set_behavior(&app.keys, KEY_PLAY, KEY_BEHAVIOR_PRESS);
    if (!app.display_on) {
        set_display(true);
    }
    app.frame_stable = false;
}

static void draw_prompt(void *context, Canvas *canvas)
{
    Screen *screen = context;
    screen->draw(screen->context, canvas);
}

static void follow_prompt_keys(uint32_t now_ms)
{
    KeyEvent event;
    Screen *screen = &app.prompt.screen;
    while (app.prompt_result < 0 && key_tracker_next_event(&app.keys, &event)) {
        ScreenAction action = screen->handle_key(screen->context, event, now_ms);
        bool dismissed =
            action.type == SCREEN_ACTION_POP || action.type == SCREEN_ACTION_POP_TO_ROOT;
        if (dismissed && app.prompt_result < 0) {
            app.prompt_result = app.prompt_default;
        }
    }
}

int app_choice_step(void)
{
    uint32_t now_ms = hal_clock_milliseconds();
    app.now_ms = now_ms;
    hal_audio_service();
    hal_bluetooth_service();
    read_input();
    key_tracker_update(&app.keys, now_ms);
    follow_prompt_keys(now_ms);
    Screen *screen = &app.prompt.screen;
    screen->update(screen->context, now_ms);
    render((FrameDrawer){.draw = draw_prompt, .context = screen}, false, STRIP_HEIGHT);
    light_after_first_frame();
    return app.prompt_result;
}

void app_choice_close(void)
{
    app.frame_stable = false;
    app_dac_keys_stop();
}

void app_dac_keys_stop(void)
{
    key_tracker_init(&app.keys, &KEY_CONFIG);
    if (screen_stack_depth(&app.stack) > 0) {
        use_screen_key_behaviors();
    }
}

void app_request_rescan(void)
{
    app.rescan_requested = true;
}

void app_request_rescan_if_music_changed(void)
{
    app.music_check_requested = true;
    app.usb_written = true;
}

static const StringId CLOCK_PROMPT_OPTIONS[] = {STR_SETTINGS_SET_NOW, STR_SETTINGS_LATER};

static int clock_prompt_current(const void *context)
{
    (void)context;
    return 0;
}

static void clock_prompt_choose(void *context, int index)
{
    AppState *state = context;
    state->open_date_time = index == 0;
}

static void push_screen(Screen *screen, uint32_t now_ms)
{
    if (screen_stack_apply(&app.stack, screen_action_push(screen), now_ms)) {
        use_screen_key_behaviors();
    }
}

static void ask_for_the_clock(uint32_t now_ms)
{
    app.clock_question = false;
    if (app.clock.clock.known) {
        return;
    }
    ChoiceConfig config = {.title = i18n_text(STR_SETTINGS_CLOCK_PROMPT),
                           .option_ids = CLOCK_PROMPT_OPTIONS,
                           .option_count = 2,
                           .current = clock_prompt_current,
                           .choose = clock_prompt_choose,
                           .context = &app,
                           .theme = &app.theme};
    push_screen(choice_screen_init(&app.clock_prompt, config), now_ms);
}

static void follow_clock_prompt(uint32_t now_ms)
{
    if (app.clock_question && !app.music_check_requested && !fade_running()) {
        ask_for_the_clock(now_ms);
    }
    if (app.open_date_time) {
        app.open_date_time = false;
        push_screen(&app.screens.date_time.screen, now_ms);
    }
}

static void take_computer_clock(void)
{
    if (!app.usb_written) {
        return;
    }
    app.usb_written = false;
    uint32_t now_ms = hal_clock_milliseconds();
    if (clock_store_after_usb(&app.clock, &app.settings.settings, now_ms)) {
        settings_store_save(&app.settings);
        toast_show(&app.toast, i18n_text(STR_SETTINGS_CLOCK_SYNCED), now_ms);
    }
}

static void draw_charge(void *context, Canvas *canvas)
{
    (void)context;
    charge_screen_draw(canvas, current_theme(), app.charge_battery);
}

static void follow_charge_keys(uint32_t now_ms)
{
    KeyEvent event;
    while (app.charging && key_tracker_next_event(&app.keys, &event)) {
        if (event.key == KEY_POWER && event.gesture == KEY_GESTURE_LONG_PRESS) {
            stop_charging(now_ms);
        } else {
            light_charge_screen(now_ms);
        }
    }
}

static void charge_step(uint32_t now_ms)
{
    follow_charge_keys(now_ms);
    player_flush_save(&app.player);
    if (!app.charging) {
        return;
    }
    if (deadline_reached(now_ms, app.battery_check_ms)) {
        app.battery_check_ms = now_ms + BATTERY_CHECK_MS;
        app.charge_battery = hal_power_battery();
        if (!app.charge_battery.usb_power) {
            app.charging = false;
            power_off_now();
            return;
        }
    }
    if (app.display_on && deadline_reached(now_ms, app.charge_lit_until_ms)) {
        set_display(false);
    }
    if (app.display_on) {
        render((FrameDrawer){.draw = draw_charge, .context = NULL}, true, STRIP_HEIGHT);
        light_after_first_frame();
    } else {
        use_wanted_clock();
    }
}

void app_step(void)
{
    if (app.powered_off) {
        return;
    }
    app.idle = false;
    take_computer_clock();
    if (app.music_check_requested && fade_finished(&app.splash, hal_clock_milliseconds())) {
        app.music_check_requested = false;
        bool changed = hal_system_restart_pending();
        if (!changed) {
            app_show_notice(i18n_text(STR_RESCAN_MUSIC_TITLE), i18n_text(STR_RESCAN_CHECKING));
            changed = !music_check_unchanged() || !library_store_open(&app.library);
        }
        if (!changed) {
            music_check_note_firmware();
        }
        app.rescan_requested |= changed;
    }
    if (app.rescan_requested) {
        app.rescan_requested = false;
        run_rescan();
        return;
    }
    uint32_t started_us = hal_clock_microseconds();
    uint32_t now_ms = hal_clock_milliseconds();
    app.now_ms = now_ms;
    hal_audio_service();
    hal_bluetooth_service();
    read_input();
    key_tracker_update(&app.keys, now_ms);
    if (app.charging) {
        charge_step(now_ms);
        return;
    }
    if (fade_running()) {
        drain_key_events();
    } else {
        dispatch_key_events(now_ms);
    }
    follow_clock_prompt(now_ms);
    follow_orientation();
    if (app.saying_goodbye && fade_finished(&app.goodbye, now_ms)) {
        finish_shut_down();
    }
    if (!app.powered_off) {
        clock_store_step(&app.clock, now_ms);
        player_update(&app.player, now_ms);
        bluetooth_link_step(&app.bluetooth, &app.player, now_ms);
        PlayerStatus status = player_status(&app.player);
        spoken_store_follow(&app.spoken, status, now_ms);
        follow_history(&status, now_ms);
        explain_refusals(now_ms);
        update_power(now_ms);
    }
    if (app.powered_off) {
        return;
    }
    if (!app.display_on) {
        use_wanted_clock();
    }
    toast_update(&app.toast, now_ms);
    update_screen(now_ms);
    frame_stats_record(&app.update_stats, hal_clock_microseconds() - started_us);
    if (app.display_on && frame_needed(now_ms)) {
        render_frame();
        light_after_first_frame();
    } else {
        app.idle = app.display_on;
    }
    app.key_activity = false;
    player_flush_save(&app.player);
    frame_stats_record(&app.frame_stats, hal_clock_microseconds() - started_us);
}

bool app_idle(void)
{
    return app.idle;
}

bool app_charging(void)
{
    return app.charging;
}

void app_show_notice(const char *title, const char *message)
{
    MessageConfig config = {.title = title, .message = message, .theme = current_theme()};
    Screen *screen = message_screen_init(&app.notice, config);
    if (!app.display_on) {
        set_display(true);
    }
    render((FrameDrawer){.draw = draw_notice, .context = screen}, false, NOTICE_STRIP_HEIGHT);
    light_after_first_frame();
}

void app_show_splash(void)
{
    app.now_ms = hal_clock_milliseconds();
    fade_start(&app.splash, app.now_ms, SPLASH_TIMING);
}

void app_wake(void)
{
    uint32_t now_ms = hal_clock_milliseconds();
    power_policy_wake(&app.power, now_ms);
    clock_store_wake(&app.clock, now_ms);
    if (app.charging) {
        light_charge_screen(now_ms);
    }
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

const char *app_now_playing_lyric(void)
{
    return now_playing_lyric(&app.screens.now_playing);
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

void app_forget_clock(void)
{
    app.clock.started = false;
}

#include "now_playing_screen.h"

#include <stddef.h>

#include "core/format.h"
#include "core/string_builder.h"
#include "ui/header.h"
#include "ui/icons.h"
#include "ui/label.h"
#include "ui/marquee.h"

enum {
    TITLE_TOP = 30,
    ARTIST_TOP = 56,
    ALBUM_TOP = 80,
    TEXT_LINE_HEIGHT = 24,
    PROGRESS_TOP = 112,
    PROGRESS_HEIGHT = 4,
    TIMES_TOP = 120,
    STATUS_TOP = 146,
    SMALL_LINE_HEIGHT = 18,
    ICON_SIZE = 10,
    ICON_GAP = 8,
    TIME_CAPACITY = 16,
    STATUS_CAPACITY = 64
};

static const char NOTHING_PLAYING[] = "Nothing playing";
static const char *const SHUFFLE_TEXT[SHUFFLE_MODE_COUNT] = {"", "Shuffle Songs", "Shuffle Albums"};
static const char *const REPEAT_TEXT[REPEAT_MODE_COUNT] = {"", "Repeat One", "Repeat All"};

static NowPlayingCommand by_gesture(KeyGesture gesture, NowPlayingCommand press,
                                    NowPlayingCommand hold)
{
    switch (gesture) {
    case KEY_GESTURE_PRESS:
        return press;
    case KEY_GESTURE_LONG_PRESS:
        return hold;
    case KEY_GESTURE_REPEAT:
        return NOW_PLAYING_NONE;
    }
    return NOW_PLAYING_NONE;
}

uint32_t now_playing_scrub_target(uint32_t position_ms, uint32_t duration_ms, int32_t delta_ms)
{
    int64_t target = (int64_t)position_ms + delta_ms;
    if (target < 0) {
        return 0;
    }
    return target > duration_ms ? duration_ms : (uint32_t)target;
}

static NowPlayingCommand scrub_command_for(KeyEvent event)
{
    bool held = event.gesture != KEY_GESTURE_PRESS;
    switch (event.key) {
    case KEY_VOLUME_UP:
        return held ? NOW_PLAYING_SCRUB_FORWARD_FAST : NOW_PLAYING_SCRUB_FORWARD;
    case KEY_VOLUME_DOWN:
        return held ? NOW_PLAYING_SCRUB_BACK_FAST : NOW_PLAYING_SCRUB_BACK;
    case KEY_PLAY:
        return by_gesture(event.gesture, NOW_PLAYING_SCRUB_COMMIT, NOW_PLAYING_NONE);
    case KEY_BACK:
        return by_gesture(event.gesture, NOW_PLAYING_SCRUB_CANCEL, NOW_PLAYING_HOME);
    case KEY_MENU:
    case KEY_POWER:
        return NOW_PLAYING_NONE;
    }
    return NOW_PLAYING_NONE;
}

NowPlayingCommand now_playing_command_for(KeyEvent event, bool scrubbing)
{
    if (scrubbing) {
        return scrub_command_for(event);
    }
    switch (event.key) {
    case KEY_VOLUME_UP:
        return by_gesture(event.gesture, NOW_PLAYING_VOLUME_UP, NOW_PLAYING_NEXT);
    case KEY_VOLUME_DOWN:
        return by_gesture(event.gesture, NOW_PLAYING_VOLUME_DOWN, NOW_PLAYING_PREVIOUS);
    case KEY_PLAY:
        return by_gesture(event.gesture, NOW_PLAYING_TOGGLE_PAUSE, NOW_PLAYING_SCRUB_START);
    case KEY_BACK:
        return by_gesture(event.gesture, NOW_PLAYING_LEAVE, NOW_PLAYING_HOME);
    case KEY_MENU:
    case KEY_POWER:
        return NOW_PLAYING_NONE;
    }
    return NOW_PLAYING_NONE;
}

int now_playing_progress_width(uint32_t position_ms, uint32_t duration_ms, int width)
{
    if (duration_ms == 0) {
        return 0;
    }
    if (position_ms >= duration_ms) {
        return width;
    }
    return (int)((uint64_t)position_ms * (uint64_t)width / duration_ms);
}

static void move_scrub(NowPlayingScreen *now_playing, int32_t delta_ms)
{
    now_playing->scrub_position_ms = now_playing_scrub_target(
        now_playing->scrub_position_ms, now_playing->status.duration_ms, delta_ms);
}

static void start_scrub(NowPlayingScreen *now_playing)
{
    now_playing->scrubbing = now_playing->status.has_track;
    now_playing->scrub_position_ms = now_playing->status.position_ms;
}

static void commit_scrub(NowPlayingScreen *now_playing)
{
    player_seek(now_playing->config.player, now_playing->scrub_position_ms);
    now_playing->scrubbing = false;
}

static ScreenAction run_scrub_command(NowPlayingScreen *now_playing, NowPlayingCommand command)
{
    switch (command) {
    case NOW_PLAYING_SCRUB_START:
        start_scrub(now_playing);
        break;
    case NOW_PLAYING_SCRUB_FORWARD:
        move_scrub(now_playing, NOW_PLAYING_SCRUB_STEP_MS);
        break;
    case NOW_PLAYING_SCRUB_FORWARD_FAST:
        move_scrub(now_playing, NOW_PLAYING_SCRUB_FAST_STEP_MS);
        break;
    case NOW_PLAYING_SCRUB_BACK:
        move_scrub(now_playing, -NOW_PLAYING_SCRUB_STEP_MS);
        break;
    case NOW_PLAYING_SCRUB_BACK_FAST:
        move_scrub(now_playing, -NOW_PLAYING_SCRUB_FAST_STEP_MS);
        break;
    case NOW_PLAYING_SCRUB_COMMIT:
        commit_scrub(now_playing);
        break;
    default:
        now_playing->scrubbing = false;
        break;
    }
    return screen_action_none();
}

static ScreenAction run_command(NowPlayingScreen *now_playing, NowPlayingCommand command)
{
    Player *player = now_playing->config.player;
    switch (command) {
    case NOW_PLAYING_TOGGLE_PAUSE:
        player_toggle_pause(player);
        break;
    case NOW_PLAYING_NEXT:
        player_skip(player, 1);
        break;
    case NOW_PLAYING_PREVIOUS:
        player_skip(player, -1);
        break;
    case NOW_PLAYING_VOLUME_UP:
        player_change_volume(player, 1);
        break;
    case NOW_PLAYING_VOLUME_DOWN:
        player_change_volume(player, -1);
        break;
    case NOW_PLAYING_LEAVE:
        return screen_action_pop();
    case NOW_PLAYING_HOME:
        now_playing->scrubbing = false;
        return screen_action_pop_to_root();
    case NOW_PLAYING_SCRUB_START:
    case NOW_PLAYING_SCRUB_FORWARD:
    case NOW_PLAYING_SCRUB_FORWARD_FAST:
    case NOW_PLAYING_SCRUB_BACK:
    case NOW_PLAYING_SCRUB_BACK_FAST:
    case NOW_PLAYING_SCRUB_COMMIT:
    case NOW_PLAYING_SCRUB_CANCEL:
        return run_scrub_command(now_playing, command);
    case NOW_PLAYING_NONE:
        break;
    }
    return screen_action_none();
}

static void load_texts(NowPlayingScreen *now_playing, uint16_t track_index)
{
    const Library *library = now_playing->config.library;
    LibraryTrack track;
    LibraryAlbum album;
    LibraryGroup artist;
    now_playing->title[0] = '\0';
    now_playing->artist[0] = '\0';
    now_playing->album[0] = '\0';
    if (!library_read_track(library, track_index, &track)) {
        return;
    }
    library_read_string(library, track.title, now_playing->title, NOW_PLAYING_TEXT_CAPACITY);
    if (library_read_group(library, LIBRARY_ARTISTS, track.artist, &artist)) {
        library_read_string(library, artist.name, now_playing->artist, NOW_PLAYING_TEXT_CAPACITY);
    }
    if (library_read_album(library, track.album, &album)) {
        library_read_string(library, album.title, now_playing->album, NOW_PLAYING_TEXT_CAPACITY);
    }
}

static void refresh(NowPlayingScreen *now_playing, uint32_t now_ms)
{
    now_playing->now_ms = now_ms;
    now_playing->status = player_status(now_playing->config.player);
    bool changed = !now_playing->has_text || now_playing->text_track != now_playing->status.track;
    if (now_playing->status.has_track && changed) {
        load_texts(now_playing, now_playing->status.track);
        now_playing->has_text = true;
        now_playing->text_track = now_playing->status.track;
        now_playing->text_changed_ms = now_ms;
    }
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    NowPlayingScreen *now_playing = context;
    ScreenAction action =
        run_command(now_playing, now_playing_command_for(event, now_playing->scrubbing));
    refresh(now_playing, now_ms);
    return action;
}

static void enter(void *context, uint32_t now_ms)
{
    NowPlayingScreen *now_playing = context;
    now_playing->has_text = false;
    now_playing->scrubbing = false;
    refresh(now_playing, now_ms);
}

static void update(void *context, uint32_t now_ms)
{
    refresh(context, now_ms);
}

static Rect line_area(const Theme *theme, int top, int height)
{
    int padding = theme->metrics.padding;
    return (Rect){padding, top, theme->metrics.screen_width - 2 * padding, height};
}

static void draw_text_line(const NowPlayingScreen *now_playing, Canvas *canvas, Label label,
                           TextStyle style)
{
    int width = text_width(style.font, label.text);
    if (width <= label.area.width) {
        label_draw(canvas, style, label);
        return;
    }
    MarqueeTiming timing = now_playing->config.theme->marquee;
    uint32_t elapsed = now_playing->now_ms - now_playing->text_changed_ms;
    int offset = marquee_offset(timing, width, label.area.width, elapsed);
    label_draw_scrolling(canvas, style, label, (Marquee){.offset = offset, .gap = timing.gap_px});
}

static void draw_track_text(const NowPlayingScreen *now_playing, Canvas *canvas)
{
    const Theme *theme = now_playing->config.theme;
    TextStyle title = {.font = theme->fonts.title, .color = theme->colors.text};
    TextStyle artist = {.font = theme->fonts.body, .color = theme->colors.text};
    TextStyle album = {.font = theme->fonts.body, .color = theme->colors.text_secondary};
    draw_text_line(now_playing, canvas,
                   (Label){line_area(theme, TITLE_TOP, TEXT_LINE_HEIGHT), now_playing->title},
                   title);
    draw_text_line(now_playing, canvas,
                   (Label){line_area(theme, ARTIST_TOP, TEXT_LINE_HEIGHT), now_playing->artist},
                   artist);
    draw_text_line(now_playing, canvas,
                   (Label){line_area(theme, ALBUM_TOP, TEXT_LINE_HEIGHT), now_playing->album},
                   album);
}

static uint32_t shown_position(const NowPlayingScreen *now_playing)
{
    return now_playing->scrubbing ? now_playing->scrub_position_ms
                                  : now_playing->status.position_ms;
}

static void draw_progress(const NowPlayingScreen *now_playing, Canvas *canvas)
{
    const Theme *theme = now_playing->config.theme;
    Rect bar = line_area(theme, PROGRESS_TOP, PROGRESS_HEIGHT);
    int filled = now_playing_progress_width(shown_position(now_playing),
                                            now_playing->status.duration_ms, bar.width);
    Color fill = now_playing->scrubbing ? theme->colors.text : theme->colors.selection_background;
    canvas_fill_rect(canvas, bar, theme->colors.divider);
    canvas_fill_rect(canvas, (Rect){bar.x, bar.y, filled, bar.height}, fill);
}

static void draw_right_aligned(Canvas *canvas, TextStyle style, Rect area, const char *text)
{
    int width = text_width(style.font, text);
    Rect right = {area.x + area.width - width, area.y, width, area.height};
    label_draw(canvas, style, (Label){.area = right, .text = text});
}

static void draw_times(const NowPlayingScreen *now_playing, Canvas *canvas)
{
    const Theme *theme = now_playing->config.theme;
    uint32_t position_ms = shown_position(now_playing);
    uint32_t duration_ms = now_playing->status.duration_ms;
    uint32_t remaining_ms = duration_ms > position_ms ? duration_ms - position_ms : 0;
    char elapsed[TIME_CAPACITY];
    char remaining[TIME_CAPACITY] = "-";
    format_duration(position_ms, elapsed, sizeof elapsed);
    format_duration(remaining_ms, remaining + 1, sizeof remaining - 1);
    Rect area = line_area(theme, TIMES_TOP, SMALL_LINE_HEIGHT);
    TextStyle style = {.font = theme->fonts.small, .color = theme->colors.text_secondary};
    label_draw(canvas, style, (Label){.area = area, .text = elapsed});
    draw_right_aligned(canvas, style, area, remaining);
}

static void draw_state_icon(const NowPlayingScreen *now_playing, Canvas *canvas, Point origin)
{
    Color color = now_playing->config.theme->colors.text;
    Rect area = {origin.x, origin.y, ICON_SIZE, ICON_SIZE};
    switch (now_playing->status.state) {
    case PLAYER_PLAYING:
        icon_draw_play(canvas, area, color);
        return;
    case PLAYER_PAUSED:
        icon_draw_pause(canvas, area, color);
        return;
    case PLAYER_IDLE:
        icon_draw_stop(canvas, area, color);
        return;
    }
}

static void modes_text(const PlayerStatus *status, char *buffer, size_t capacity)
{
    StringBuilder builder = string_builder_make(buffer, capacity);
    string_builder_append(&builder, SHUFFLE_TEXT[status->shuffle]);
    if (status->shuffle != SHUFFLE_OFF && status->repeat != REPEAT_OFF) {
        string_builder_append(&builder, " \xC2\xB7 ");
    }
    string_builder_append(&builder, REPEAT_TEXT[status->repeat]);
}

static void draw_status(const NowPlayingScreen *now_playing, Canvas *canvas)
{
    const Theme *theme = now_playing->config.theme;
    Rect area = line_area(theme, STATUS_TOP, SMALL_LINE_HEIGHT);
    TextStyle style = {.font = theme->fonts.small, .color = theme->colors.text_secondary};
    char modes[STATUS_CAPACITY];
    char volume[STATUS_CAPACITY];
    StringBuilder builder = string_builder_make(volume, sizeof volume);
    string_builder_append(&builder, "Vol ");
    string_builder_append_unsigned(&builder, now_playing->status.volume);
    modes_text(&now_playing->status, modes, sizeof modes);
    if (now_playing->scrubbing) {
        StringBuilder seeking = string_builder_make(modes, sizeof modes);
        string_builder_append(&seeking, "Seeking \xC2\xB7 Play to jump");
    }
    draw_state_icon(now_playing, canvas, (Point){area.x, area.y + (area.height - ICON_SIZE) / 2});
    Rect modes_area = {area.x + ICON_SIZE + ICON_GAP, area.y, area.width / 2, area.height};
    label_draw(canvas, style, (Label){.area = modes_area, .text = modes});
    draw_right_aligned(canvas, style, area, volume);
}

static void draw_nothing_playing(const Theme *theme, Canvas *canvas)
{
    TextStyle style = {.font = theme->fonts.body, .color = theme->colors.text_secondary};
    Rect content = theme_content_area(theme);
    Rect area = label_centered_area(style.font, content, NOTHING_PLAYING);
    label_draw(canvas, style, (Label){.area = area, .text = NOTHING_PLAYING});
}

static void draw(void *context, Canvas *canvas)
{
    const NowPlayingScreen *now_playing = context;
    const Theme *theme = now_playing->config.theme;
    canvas_fill_rect(canvas, theme_content_area(theme), theme->colors.background);
    header_draw(canvas, theme, now_playing->screen.title);
    if (!now_playing->status.has_track) {
        draw_nothing_playing(theme, canvas);
        return;
    }
    draw_track_text(now_playing, canvas);
    draw_progress(now_playing, canvas);
    draw_times(now_playing, canvas);
    draw_status(now_playing, canvas);
}

Screen *now_playing_screen_init(NowPlayingScreen *now_playing, NowPlayingConfig config)
{
    now_playing->config = config;
    now_playing->has_text = false;
    now_playing->scrubbing = false;
    now_playing->scrub_position_ms = 0;
    now_playing->status = (PlayerStatus){.state = PLAYER_IDLE};
    now_playing->screen = (Screen){
        .title = "Now Playing",
        .context = now_playing,
        .hold_volume_keys = true,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &now_playing->screen;
}

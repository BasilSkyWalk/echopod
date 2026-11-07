#include "now_playing_screen.h"

#include <stddef.h>

#include "core/format.h"
#include "core/i18n.h"
#include "core/string_builder.h"
#include "gfx/image.h"
#include "hal/audio.h"
#include "hal/storage.h"
#include "screens/equalizer_screen.h"
#include "settings/settings.h"
#include "ui/header.h"
#include "ui/icons.h"
#include "ui/label.h"
#include "ui/marquee.h"

enum {
    COVER_TOP = 30,
    COVER_GAP = 12,
    TITLE_TOP = 28,
    ARTIST_TOP = 50,
    ALBUM_TOP = 70,
    TEXT_LINE_HEIGHT = 22,
    FORMAT_TOP = 97,
    DETAIL_TOP = 116,
    MODES_TOP = 131,
    SMALL_LINE_HEIGHT = 16,
    CODEC_PADDING = 4,
    CODEC_GAP = 7,
    OUTLINE = 1,
    PROGRESS_TOP = 148,
    PROGRESS_HEIGHT = 3,
    TIMES_TOP = 153,
    TIME_CAPACITY = 16,
    BADGE_SIZE = 40,
    BADGE_ICON_SIZE = 18,
    STATUS_CAPACITY = 64,
    BITRATE_CAPACITY = 24,
    RATE_CAPACITY = 16,
    PORTRAIT_COVER_TOP = 52,
    PORTRAIT_COVER_MIN_GAP = 6,
    PORTRAIT_TEXT_GAP = 10,
    PORTRAIT_FORMAT_GAP = 4,
    PORTRAIT_BOTTOM_MARGIN = 2,
    PORTRAIT_PROGRESS_GAP = 3,
    LYRICS_TOP = 49,
    LYRICS_ROWS = 5,
    LYRICS_ROW_HEIGHT = 19,
    EXTENSION_ROOM = 5,
    TIMES_GAP = TIMES_TOP - PROGRESS_TOP - PROGRESS_HEIGHT
};

typedef struct Layout {
    int cover_left;
    int cover_top;
    int title_top;
    int format_top;
    int detail_top;
    int modes_top;
    int progress_top;
    int times_top;
    bool centered;
} Layout;

static const char SEPARATOR[] = " \xC2\xB7 ";

static const StringId SHUFFLE_TEXT[SHUFFLE_MODE_COUNT - 1] = {STR_MENU_SHUFFLE_SONGS,
                                                              STR_MENU_SHUFFLE_ALBUMS};
static const StringId REPEAT_TEXT[REPEAT_MODE_COUNT - 1] = {STR_MENU_REPEAT_ONE,
                                                            STR_MENU_REPEAT_ALL};

const char *now_playing_codec_name(HalAudioCodec codec)
{
    switch (codec) {
    case HAL_AUDIO_CODEC_FLAC:
        return "FLAC";
    case HAL_AUDIO_CODEC_MP3:
        return "MP3";
    case HAL_AUDIO_CODEC_WAV:
        return "WAV";
    case HAL_AUDIO_CODEC_ALAC:
        return "ALAC";
    case HAL_AUDIO_CODEC_AAC:
        return "AAC";
    case HAL_AUDIO_CODEC_OPUS:
        return "Opus";
    case HAL_AUDIO_CODEC_UNKNOWN:
        return "";
    }
    return "";
}

void now_playing_stream_text(HalAudioFormat format, char *buffer, size_t capacity)
{
    StringBuilder builder = string_builder_make(buffer, capacity);
    if (format.codec == HAL_AUDIO_CODEC_UNKNOWN || format.sample_rate == 0) {
        return;
    }
    if (format.bits_per_sample > 0) {
        string_builder_append_unsigned(&builder, format.bits_per_sample);
        string_builder_append(&builder, "-bit");
        string_builder_append(&builder, SEPARATOR);
    }
    char rate[RATE_CAPACITY];
    format_sample_rate(format.sample_rate, rate, sizeof rate);
    string_builder_append(&builder, rate);
}

void now_playing_detail_text(const PlayerStatus *status, char *buffer, size_t capacity)
{
    StringBuilder builder = string_builder_make(buffer, capacity);
    bool has_bitrate =
        status->format.codec != HAL_AUDIO_CODEC_UNKNOWN && status->format.bitrate_kbps > 0;
    if (has_bitrate) {
        char bitrate[BITRATE_CAPACITY];
        format_bitrate(status->format.bitrate_kbps, bitrate, sizeof bitrate);
        string_builder_append(&builder, bitrate);
    }
    if (status->place.total == 0) {
        return;
    }
    if (has_bitrate) {
        string_builder_append(&builder, SEPARATOR);
    }
    char number[RATE_CAPACITY];
    char total[RATE_CAPACITY];
    StringBuilder number_digits = string_builder_make(number, sizeof number);
    StringBuilder total_digits = string_builder_make(total, sizeof total);
    string_builder_append_unsigned(&number_digits, status->place.number);
    string_builder_append_unsigned(&total_digits, status->place.total);
    const char *arguments[] = {number, total};
    i18n_format(&builder, STR_MENU_POSITION_OF, arguments, 2);
}

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

NowPlayingControls now_playing_controls_of(const uint8_t *options)
{
    if (options == NULL) {
        return (NowPlayingControls){0};
    }
    return (NowPlayingControls){.rocker = options[OPTION_ROCKER],
                                .menu_skips = options[OPTION_MENU_KEY] == MENU_KEY_NEXT};
}

static NowPlayingCommand rocker_command(KeyGesture gesture, NowPlayingControls controls, bool up)
{
    NowPlayingCommand volume = up ? NOW_PLAYING_VOLUME_UP : NOW_PLAYING_VOLUME_DOWN;
    NowPlayingCommand skip = up ? NOW_PLAYING_NEXT : NOW_PLAYING_PREVIOUS;
    bool held = gesture != KEY_GESTURE_PRESS;
    if (controls.rocker == ROCKER_SKIP_VOLUME) {
        return held ? volume : skip;
    }
    if (held && controls.volume_shown) {
        return volume;
    }
    if (held && controls.rocker == ROCKER_VOLUME_SEEK) {
        return up ? NOW_PLAYING_SEEK_FORWARD : NOW_PLAYING_SEEK_BACK;
    }
    return by_gesture(gesture, volume, skip);
}

NowPlayingCommand now_playing_command_for(KeyEvent event, bool scrubbing)
{
    return now_playing_command_with(event, scrubbing, (NowPlayingControls){0});
}

NowPlayingCommand now_playing_command_with(KeyEvent event, bool scrubbing,
                                           NowPlayingControls controls)
{
    if (scrubbing) {
        return scrub_command_for(event);
    }
    switch (event.key) {
    case KEY_VOLUME_UP:
    case KEY_VOLUME_DOWN:
        return rocker_command(event.gesture, controls, event.key == KEY_VOLUME_UP);
    case KEY_PLAY:
        return by_gesture(event.gesture, NOW_PLAYING_TOGGLE_PAUSE, NOW_PLAYING_SCRUB_START);
    case KEY_BACK:
        return by_gesture(event.gesture, NOW_PLAYING_LEAVE, NOW_PLAYING_HOME);
    case KEY_MENU:
        return controls.menu_skips
                   ? by_gesture(event.gesture, NOW_PLAYING_NEXT, NOW_PLAYING_UP_NEXT)
                   : by_gesture(event.gesture, NOW_PLAYING_UP_NEXT, NOW_PLAYING_EQUALIZER);
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

NowPlayingFeedback now_playing_feedback_for(NowPlayingCommand command, PlayerState after)
{
    switch (command) {
    case NOW_PLAYING_VOLUME_UP:
    case NOW_PLAYING_VOLUME_DOWN:
        return NOW_PLAYING_FEEDBACK_VOLUME;
    case NOW_PLAYING_NEXT:
        return NOW_PLAYING_FEEDBACK_NEXT;
    case NOW_PLAYING_PREVIOUS:
        return NOW_PLAYING_FEEDBACK_PREVIOUS;
    case NOW_PLAYING_TOGGLE_PAUSE:
        return after == PLAYER_PAUSED ? NOW_PLAYING_FEEDBACK_PAUSE : NOW_PLAYING_FEEDBACK_PLAY;
    default:
        return NOW_PLAYING_FEEDBACK_NONE;
    }
}

bool now_playing_feedback_visible(NowPlayingFeedback feedback, uint32_t started_ms, uint32_t now_ms)
{
    if (feedback == NOW_PLAYING_FEEDBACK_NONE) {
        return false;
    }
    uint32_t duration = feedback == NOW_PLAYING_FEEDBACK_VOLUME ? NOW_PLAYING_VOLUME_FEEDBACK_MS
                                                                : NOW_PLAYING_BADGE_MS;
    return now_ms - started_ms < duration;
}

int now_playing_volume_width(uint8_t volume, int width)
{
    uint8_t level = volume < SETTINGS_VOLUME_MAX ? volume : SETTINGS_VOLUME_MAX;
    return level * width / SETTINGS_VOLUME_MAX;
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
    case NOW_PLAYING_SEEK_FORWARD:
    case NOW_PLAYING_SEEK_BACK:
        player_jump(player, command == NOW_PLAYING_SEEK_FORWARD ? NOW_PLAYING_SCRUB_STEP_MS
                                                                : -NOW_PLAYING_SCRUB_STEP_MS);
        break;
    case NOW_PLAYING_LEAVE:
        return screen_action_pop();
    case NOW_PLAYING_UP_NEXT:
        return now_playing->config.up_next != NULL ? screen_action_push(now_playing->config.up_next)
                                                   : screen_action_none();
    case NOW_PLAYING_EQUALIZER:
        return now_playing->config.equalizer != NULL
                   ? equalizer_screen_open(now_playing->config.equalizer->context)
                   : screen_action_none();
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

static bool lyrics_wanted(const NowPlayingScreen *now_playing)
{
    const uint8_t *options = now_playing->config.options;
    return options != NULL && options[OPTION_LYRICS] == LYRICS_ON;
}

static bool open_lyrics(char *path, size_t dot, HalFile *file)
{
    static const char *const EXTENSIONS[] = {".lrc", ".LRC"};
    for (size_t index = 0; index < sizeof EXTENSIONS / sizeof EXTENSIONS[0]; index++) {
        for (size_t at = 0; at < EXTENSION_ROOM; at++) {
            path[dot + at] = EXTENSIONS[index][at];
        }
        if (hal_file_open(file, path)) {
            return true;
        }
    }
    return false;
}

static void read_lyrics(Lyrics *lyrics, HalFile file)
{
    char bytes[NOW_PLAYING_LYRICS_CHUNK];
    uint32_t end =
        file.size < NOW_PLAYING_LYRICS_READ_LIMIT ? file.size : NOW_PLAYING_LYRICS_READ_LIMIT;
    for (uint32_t offset = 0; offset < end && !lyrics_full(lyrics); offset += sizeof bytes) {
        uint32_t length = end - offset < sizeof bytes ? end - offset : sizeof bytes;
        if (!hal_file_read(file, offset, bytes, length)) {
            break;
        }
        lyrics_feed(lyrics, bytes, length);
        hal_audio_service();
    }
    lyrics_finish(lyrics);
}

static void load_lyrics(NowPlayingScreen *now_playing, const LibraryTrack *track)
{
    NowPlayingLyrics *lyrics = &now_playing->lyrics;
    lyrics->ready = false;
    lyrics->current = LYRICS_NONE;
    lyrics_start(&lyrics->lyrics);
    char path[PLAYER_PATH_CAPACITY] = "/";
    if (!lyrics_wanted(now_playing) ||
        !library_read_string(now_playing->config.library, track->path, path + 1,
                             sizeof path - 1 - EXTENSION_ROOM)) {
        return;
    }
    size_t dot = 0;
    size_t length = 0;
    for (; path[length] != '\0'; length++) {
        dot = path[length] == '.' ? length : path[length] == '/' ? 0 : dot;
    }
    HalFile file;
    if (open_lyrics(path, dot > 0 ? dot : length, &file)) {
        read_lyrics(&lyrics->lyrics, file);
        hal_file_close(&file);
        lyrics->ready = true;
    }
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
    now_playing->lyrics.ready = false;
    if (!library_read_track(library, track_index, &track)) {
        return;
    }
    load_lyrics(now_playing, &track);
    library_read_string(library, track.title, now_playing->title, NOW_PLAYING_TEXT_CAPACITY);
    if (library_read_group(library, LIBRARY_ARTISTS, track.artist, &artist)) {
        library_read_string(library, artist.name, now_playing->artist, NOW_PLAYING_TEXT_CAPACITY);
    }
    now_playing->album_index = track.album;
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
    NowPlayingControls controls = now_playing_controls_of(now_playing->config.options);
    controls.volume_shown = now_playing->feedback == NOW_PLAYING_FEEDBACK_VOLUME &&
                            now_ms - now_playing->feedback_started_ms < NOW_PLAYING_VOLUME_HOLD_MS;
    now_playing->now_ms = now_ms;
    NowPlayingCommand command = now_playing_command_with(event, now_playing->scrubbing, controls);
    ScreenAction action = run_command(now_playing, command);
    refresh(now_playing, now_ms);
    NowPlayingFeedback feedback = now_playing_feedback_for(command, now_playing->status.state);
    if (feedback != NOW_PLAYING_FEEDBACK_NONE) {
        now_playing->feedback = feedback;
        now_playing->feedback_started_ms = now_ms;
    }
    return action;
}

static void enter(void *context, uint32_t now_ms)
{
    NowPlayingScreen *now_playing = context;
    now_playing->has_text = false;
    now_playing->scrubbing = false;
    now_playing->feedback = NOW_PLAYING_FEEDBACK_NONE;
    refresh(now_playing, now_ms);
}

static bool showing(const NowPlayingScreen *now_playing, NowPlayingFeedback feedback)
{
    return now_playing->feedback == feedback &&
           now_playing_feedback_visible(feedback, now_playing->feedback_started_ms,
                                        now_playing->now_ms);
}

static void follow_lyrics(NowPlayingScreen *now_playing, uint32_t now_ms)
{
    NowPlayingLyrics *lyrics = &now_playing->lyrics;
    int current = lyrics_current(&lyrics->lyrics, now_playing->status.position_ms,
                                 now_playing->status.duration_ms);
    if (current != lyrics->current) {
        lyrics->current = current;
        lyrics->current_ms = now_ms;
    }
}

static bool lyrics_shown(const NowPlayingScreen *now_playing)
{
    return now_playing->lyrics.ready && now_playing->lyrics.lyrics.count > 0 &&
           !now_playing->scrubbing && lyrics_wanted(now_playing);
}

const char *now_playing_lyric(const NowPlayingScreen *now_playing)
{
    return lyrics_shown(now_playing)
               ? lyrics_line(&now_playing->lyrics.lyrics, now_playing->lyrics.current)
               : NULL;
}

static void update(void *context, uint32_t now_ms)
{
    NowPlayingScreen *now_playing = context;
    refresh(now_playing, now_ms);
    follow_lyrics(now_playing, now_ms);
    if (now_playing->config.covers != NULL && now_playing->status.has_track) {
        cover_cache_load_full(now_playing->config.covers, now_playing->album_index);
    }
}

static bool current_cover(const NowPlayingScreen *now_playing, Image *image)
{
    const CoverCache *covers = now_playing->config.covers;
    return covers != NULL && now_playing->has_text &&
           cover_cache_full(covers, now_playing->album_index, image) == COVER_LOADED;
}

static Layout layout_of(const NowPlayingScreen *now_playing)
{
    const Theme *theme = now_playing->config.theme;
    if (!theme_is_portrait(theme)) {
        return (Layout){
            theme->metrics.padding, COVER_TOP, TITLE_TOP, FORMAT_TOP, DETAIL_TOP, MODES_TOP,
            PROGRESS_TOP,           TIMES_TOP, false};
    }
    Image ignored;
    int cover_top = theme->metrics.header_height + PORTRAIT_COVER_MIN_GAP;
    cover_top = cover_top > PORTRAIT_COVER_TOP ? cover_top : PORTRAIT_COVER_TOP;
    int title_top = cover_top + COVER_FULL_SIZE + PORTRAIT_TEXT_GAP;
    if (!current_cover(now_playing, &ignored) && !lyrics_shown(now_playing)) {
        title_top = cover_top + (COVER_FULL_SIZE + PORTRAIT_TEXT_GAP) / 2;
    }
    int times_top = theme->metrics.screen_height - SMALL_LINE_HEIGHT - PORTRAIT_BOTTOM_MARGIN;
    int progress_top = times_top - TIMES_GAP - PROGRESS_HEIGHT;
    int modes_top = progress_top - PORTRAIT_PROGRESS_GAP - SMALL_LINE_HEIGHT;
    return (Layout){
        .cover_left = (theme->metrics.screen_width - COVER_FULL_SIZE) / 2,
        .cover_top = cover_top,
        .title_top = title_top,
        .format_top = title_top + 3 * TEXT_LINE_HEIGHT + PORTRAIT_FORMAT_GAP,
        .detail_top = modes_top - SMALL_LINE_HEIGHT,
        .modes_top = modes_top,
        .progress_top = progress_top,
        .times_top = times_top,
        .centered = true,
    };
}

static int text_left(const NowPlayingScreen *now_playing)
{
    Image ignored;
    int padding = now_playing->config.theme->metrics.padding;
    if (theme_is_portrait(now_playing->config.theme) || !current_cover(now_playing, &ignored)) {
        return padding;
    }
    return padding + COVER_FULL_SIZE + COVER_GAP;
}

static Label centered_if(bool centered, const Font *font, Label label)
{
    int width = text_width(font, label.text);
    if (centered && width < label.area.width) {
        label.area.x += (label.area.width - width) / 2;
        label.area.width = width;
    }
    return label;
}

static Rect column_area(const NowPlayingScreen *now_playing, int top, int height)
{
    const Theme *theme = now_playing->config.theme;
    int left = text_left(now_playing);
    return (Rect){left, top, theme->metrics.screen_width - left - theme->metrics.padding, height};
}

static void draw_cover(const NowPlayingScreen *now_playing, Canvas *canvas)
{
    Image cover;
    Layout layout = layout_of(now_playing);
    if (!current_cover(now_playing, &cover) || (layout.centered && lyrics_shown(now_playing))) {
        return;
    }
    image_draw_quad(canvas, cover,
                    (Quad){layout.cover_left, COVER_FULL_SIZE, layout.cover_top, COVER_FULL_SIZE,
                           layout.cover_top, COVER_FULL_SIZE});
}

static Rect line_area(const Theme *theme, int top, int height)
{
    int padding = theme->metrics.padding;
    return (Rect){padding, top, theme->metrics.screen_width - 2 * padding, height};
}

static void draw_moving_line(const NowPlayingScreen *now_playing, Canvas *canvas, Label label,
                             TextStyle style, uint32_t since_ms)
{
    int width = text_width(style.font, label.text);
    if (width <= label.area.width) {
        label_draw(canvas, style, centered_if(layout_of(now_playing).centered, style.font, label));
        return;
    }
    MarqueeTiming timing = now_playing->config.theme->marquee;
    uint32_t elapsed = now_playing->now_ms - since_ms;
    int offset = marquee_offset(timing, width, label.area.width, elapsed);
    label_draw_scrolling(canvas, style, label, (Marquee){.offset = offset, .gap = timing.gap_px});
}

static void draw_text_line(const NowPlayingScreen *now_playing, Canvas *canvas, Label label,
                           TextStyle style)
{
    draw_moving_line(now_playing, canvas, label, style, now_playing->text_changed_ms);
}

static Rect lyrics_area(const NowPlayingScreen *now_playing)
{
    const Theme *theme = now_playing->config.theme;
    Layout layout = layout_of(now_playing);
    int height = LYRICS_ROWS * LYRICS_ROW_HEIGHT;
    if (!layout.centered) {
        return column_area(now_playing, LYRICS_TOP, height);
    }
    return (Rect){theme->metrics.padding, layout.cover_top + (COVER_FULL_SIZE - height) / 2,
                  theme->metrics.screen_width - 2 * theme->metrics.padding, height};
}

static void draw_lyrics(const NowPlayingScreen *now_playing, Canvas *canvas)
{
    const Theme *theme = now_playing->config.theme;
    const NowPlayingLyrics *lyrics = &now_playing->lyrics;
    Rect area = lyrics_area(now_playing);
    bool synced = lyrics->lyrics.synced;
    for (int row = 0; row < LYRICS_ROWS; row++) {
        int index = lyrics->current - LYRICS_ROWS / 2 + row;
        if (index < 0 || index >= lyrics->lyrics.count) {
            continue;
        }
        Label label = {{area.x, area.y + row * LYRICS_ROW_HEIGHT, area.width, LYRICS_ROW_HEIGHT},
                       lyrics_line(&lyrics->lyrics, index)};
        if (synced && index == lyrics->current) {
            TextStyle style = {.font = theme->fonts.title, .color = theme->colors.text};
            draw_moving_line(now_playing, canvas, label, style, lyrics->current_ms);
            continue;
        }
        TextStyle style = {.font = theme->fonts.body,
                           .color =
                               synced ? theme->colors.text_tertiary : theme->colors.text_secondary};
        label_draw(canvas, style, centered_if(layout_of(now_playing).centered, style.font, label));
    }
}

static void draw_track_text(const NowPlayingScreen *now_playing, Canvas *canvas)
{
    const Theme *theme = now_playing->config.theme;
    TextStyle title = {.font = theme->fonts.title, .color = theme->colors.text};
    TextStyle artist = {.font = theme->fonts.body, .color = theme->colors.text_secondary};
    TextStyle album = {.font = theme->fonts.body, .color = theme->colors.text_tertiary};
    int top = layout_of(now_playing).title_top;
    draw_text_line(now_playing, canvas,
                   (Label){column_area(now_playing, top, TEXT_LINE_HEIGHT), now_playing->title},
                   title);
    if (!layout_of(now_playing).centered && lyrics_shown(now_playing)) {
        return;
    }
    draw_text_line(now_playing, canvas,
                   (Label){column_area(now_playing, top + ARTIST_TOP - TITLE_TOP, TEXT_LINE_HEIGHT),
                           now_playing->artist},
                   artist);
    draw_text_line(now_playing, canvas,
                   (Label){column_area(now_playing, top + ALBUM_TOP - TITLE_TOP, TEXT_LINE_HEIGHT),
                           now_playing->album},
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
    Rect bar = line_area(theme, layout_of(now_playing).progress_top, PROGRESS_HEIGHT);
    bool volume = showing(now_playing, NOW_PLAYING_FEEDBACK_VOLUME);
    int filled = volume ? now_playing_volume_width(now_playing->status.volume, bar.width)
                        : now_playing_progress_width(shown_position(now_playing),
                                                     now_playing->status.duration_ms, bar.width);
    Color fill =
        now_playing->scrubbing || volume ? theme->colors.selection_background : theme->colors.text;
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
    if (showing(now_playing, NOW_PLAYING_FEEDBACK_VOLUME)) {
        StringBuilder label = string_builder_make(elapsed, sizeof elapsed);
        string_builder_append(&label, i18n_text(STR_MENU_VOLUME));
        StringBuilder level = string_builder_make(remaining, sizeof remaining);
        string_builder_append_unsigned(&level, now_playing->status.volume);
    }
    Rect area = line_area(theme, layout_of(now_playing).times_top, SMALL_LINE_HEIGHT);
    TextStyle style = {.font = theme->fonts.small, .color = theme->colors.text_secondary};
    label_draw(canvas, style, (Label){.area = area, .text = elapsed});
    draw_right_aligned(canvas, style, area, remaining);
}

static void modes_text(const PlayerStatus *status, char *buffer, size_t capacity)
{
    StringBuilder builder = string_builder_make(buffer, capacity);
    if (status->shuffle != SHUFFLE_OFF) {
        string_builder_append(&builder, i18n_text(SHUFFLE_TEXT[status->shuffle - 1]));
    }
    if (status->shuffle != SHUFFLE_OFF && status->repeat != REPEAT_OFF) {
        string_builder_append(&builder, SEPARATOR);
    }
    if (status->repeat != REPEAT_OFF) {
        string_builder_append(&builder, i18n_text(REPEAT_TEXT[status->repeat - 1]));
    }
}

static void draw_outline(Canvas *canvas, Rect area, Color color)
{
    canvas_fill_rect(canvas, (Rect){area.x, area.y, area.width, OUTLINE}, color);
    canvas_fill_rect(canvas, (Rect){area.x, area.y + area.height - OUTLINE, area.width, OUTLINE},
                     color);
    canvas_fill_rect(canvas, (Rect){area.x, area.y, OUTLINE, area.height}, color);
    canvas_fill_rect(canvas, (Rect){area.x + area.width - OUTLINE, area.y, OUTLINE, area.height},
                     color);
}

static void draw_format(const NowPlayingScreen *now_playing, Canvas *canvas)
{
    const Theme *theme = now_playing->config.theme;
    HalAudioFormat format = now_playing->status.format;
    const char *codec = now_playing_codec_name(format.codec);
    if (codec[0] == '\0') {
        return;
    }
    TextStyle style = {.font = theme->fonts.small, .color = theme->colors.text_secondary};
    Rect row = column_area(now_playing, layout_of(now_playing).format_top, SMALL_LINE_HEIGHT);
    int codec_width = text_width(style.font, codec) + 2 * CODEC_PADDING;
    char stream[STATUS_CAPACITY];
    now_playing_stream_text(format, stream, sizeof stream);
    int total = codec_width + CODEC_GAP + text_width(style.font, stream);
    if (layout_of(now_playing).centered && total < row.width) {
        row.x += (row.width - total) / 2;
        row.width = total;
    }
    Rect badge = {row.x, row.y, codec_width, row.height};
    draw_outline(canvas, badge, theme->colors.text_secondary);
    Rect codec_text = label_centered_area(style.font, badge, codec);
    label_draw(canvas, style, (Label){.area = codec_text, .text = codec});
    int stream_left = badge.x + badge.width + CODEC_GAP;
    Rect stream_area = {stream_left, row.y, row.x + row.width - stream_left, row.height};
    label_draw(canvas, style, (Label){.area = stream_area, .text = stream});
}

static void draw_details(const NowPlayingScreen *now_playing, Canvas *canvas)
{
    const Theme *theme = now_playing->config.theme;
    TextStyle style = {.font = theme->fonts.small, .color = theme->colors.text_tertiary};
    Layout layout = layout_of(now_playing);
    char detail[STATUS_CAPACITY];
    now_playing_detail_text(&now_playing->status, detail, sizeof detail);
    label_draw(canvas, style,
               centered_if(layout.centered, style.font,
                           (Label){column_area(now_playing, layout.detail_top, SMALL_LINE_HEIGHT),
                                   detail}));
    char modes[STATUS_CAPACITY];
    modes_text(&now_playing->status, modes, sizeof modes);
    if (now_playing->scrubbing) {
        StringBuilder seeking = string_builder_make(modes, sizeof modes);
        string_builder_append(&seeking, i18n_text(STR_MENU_SEEKING_HINT));
    }
    label_draw(
        canvas, style,
        centered_if(layout.centered, style.font,
                    (Label){column_area(now_playing, layout.modes_top, SMALL_LINE_HEIGHT), modes}));
}

static void draw_badge_icon(Canvas *canvas, NowPlayingFeedback feedback, Rect area, Color color)
{
    switch (feedback) {
    case NOW_PLAYING_FEEDBACK_NEXT:
        icon_draw_next(canvas, area, color);
        return;
    case NOW_PLAYING_FEEDBACK_PREVIOUS:
        icon_draw_previous(canvas, area, color);
        return;
    case NOW_PLAYING_FEEDBACK_PAUSE:
        icon_draw_pause(canvas, area, color);
        return;
    case NOW_PLAYING_FEEDBACK_PLAY:
        icon_draw_play(canvas, area, color);
        return;
    default:
        return;
    }
}

static void draw_badge(const NowPlayingScreen *now_playing, Canvas *canvas)
{
    Image cover;
    NowPlayingFeedback feedback = now_playing->feedback;
    if (feedback == NOW_PLAYING_FEEDBACK_VOLUME || !showing(now_playing, feedback) ||
        !current_cover(now_playing, &cover) ||
        (layout_of(now_playing).centered && lyrics_shown(now_playing))) {
        return;
    }
    const Theme *theme = now_playing->config.theme;
    Layout layout = layout_of(now_playing);
    int center_x = layout.cover_left + COVER_FULL_SIZE / 2;
    int center_y = layout.cover_top + COVER_FULL_SIZE / 2;
    Rect badge = {center_x - BADGE_SIZE / 2, center_y - BADGE_SIZE / 2, BADGE_SIZE, BADGE_SIZE};
    Rect icon = {center_x - BADGE_ICON_SIZE / 2, center_y - BADGE_ICON_SIZE / 2, BADGE_ICON_SIZE,
                 BADGE_ICON_SIZE};
    canvas_fill_rect(canvas, badge, theme->colors.background);
    draw_badge_icon(canvas, feedback, icon, theme->colors.text);
}

static void draw_nothing_playing(const Theme *theme, Canvas *canvas)
{
    TextStyle style = {.font = theme->fonts.body, .color = theme->colors.text_secondary};
    Rect content = theme_content_area(theme);
    const char *message = i18n_text(STR_MENU_NOTHING_PLAYING);
    Rect area = label_centered_area(style.font, content, message);
    label_draw(canvas, style, (Label){.area = area, .text = message});
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
    draw_cover(now_playing, canvas);
    draw_badge(now_playing, canvas);
    draw_track_text(now_playing, canvas);
    draw_progress(now_playing, canvas);
    draw_times(now_playing, canvas);
    if (lyrics_shown(now_playing)) {
        draw_lyrics(now_playing, canvas);
        if (!layout_of(now_playing).centered) {
            return;
        }
    }
    draw_format(now_playing, canvas);
    draw_details(now_playing, canvas);
}

Screen *now_playing_screen_init(NowPlayingScreen *now_playing, NowPlayingConfig config)
{
    now_playing->config = config;
    now_playing->has_text = false;
    now_playing->scrubbing = false;
    now_playing->scrub_position_ms = 0;
    now_playing->feedback = NOW_PLAYING_FEEDBACK_NONE;
    now_playing->feedback_started_ms = 0;
    now_playing->lyrics.ready = false;
    now_playing->lyrics.current = LYRICS_NONE;
    now_playing->status = (PlayerStatus){.state = PLAYER_IDLE};
    now_playing->screen = (Screen){
        .title = i18n_text(STR_MENU_NOW_PLAYING),
        .context = now_playing,
        .hold_volume_keys = true,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &now_playing->screen;
}

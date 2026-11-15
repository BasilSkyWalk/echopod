#include "now_playing_screen.h"

#include <stddef.h>

#include "core/format.h"
#include "core/i18n.h"
#include "core/string_builder.h"
#include "gfx/image.h"
#include "hal/audio.h"
#include "hal/storage.h"
#include "hal/system.h"
#include "settings/settings.h"
#include "theme_store.h"
#include "ui/backdrop.h"
#include "ui/box.h"
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
    TIMES_GAP = TIMES_TOP - PROGRESS_TOP - PROGRESS_HEIGHT,
    FACE_STREAM_CAPACITY = 24,
    FACE_DETAIL_CAPACITY = 40,
    FACE_MODES_CAPACITY = 64,
    FACE_TEXTS_CAPACITY = FACE_STREAM_CAPACITY + FACE_DETAIL_CAPACITY + FACE_MODES_CAPACITY
};

typedef struct Layout {
    Rect boxes[THEME_LAYOUT_COUNT];
    uint8_t aligns[THEME_LAYOUT_COUNT];
    bool lyrics;
} Layout;

typedef struct Line {
    Label label;
    TextStyle style;
    uint8_t align;
} Line;

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
    case HAL_AUDIO_CODEC_QOA:
        return "QOA";
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
    bool chapters = status->chapters > 0;
    uint32_t place_total = chapters ? status->chapters : status->place.total;
    if (place_total == 0) {
        return;
    }
    if (has_bitrate) {
        string_builder_append(&builder, SEPARATOR);
    }
    char number[RATE_CAPACITY];
    char total[RATE_CAPACITY];
    StringBuilder number_digits = string_builder_make(number, sizeof number);
    StringBuilder total_digits = string_builder_make(total, sizeof total);
    string_builder_append_unsigned(&number_digits,
                                   chapters ? status->chapter + 1u : status->place.number);
    string_builder_append_unsigned(&total_digits, place_total);
    const char *arguments[] = {number, total};
    i18n_format(&builder, chapters ? STR_MENU_CHAPTER_PROGRESS : STR_MENU_POSITION_OF, arguments,
                2);
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
                   ? screen_action_push(now_playing->config.equalizer)
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

static Rect row_of(Rect column, int top, int height)
{
    return (Rect){column.x, top, column.width, height};
}

static int text_left(const NowPlayingScreen *now_playing)
{
    Image ignored;
    int padding = now_playing->config.theme->metrics.padding;
    if (!current_cover(now_playing, &ignored)) {
        return padding;
    }
    return padding + COVER_FULL_SIZE + COVER_GAP;
}

static void place_text(Layout *layout, Rect column, int title_top, int format_top)
{
    layout->boxes[THEME_LAYOUT_TITLE] = row_of(column, title_top, TEXT_LINE_HEIGHT);
    layout->boxes[THEME_LAYOUT_ARTIST] =
        row_of(column, title_top + ARTIST_TOP - TITLE_TOP, TEXT_LINE_HEIGHT);
    layout->boxes[THEME_LAYOUT_ALBUM] =
        row_of(column, title_top + ALBUM_TOP - TITLE_TOP, TEXT_LINE_HEIGHT);
    layout->boxes[THEME_LAYOUT_FORMAT] = row_of(column, format_top, SMALL_LINE_HEIGHT);
}

static void place_bottom(Layout *layout, Rect line, int modes_top, int progress_top)
{
    layout->boxes[THEME_LAYOUT_DETAIL] =
        row_of(line, modes_top - SMALL_LINE_HEIGHT, SMALL_LINE_HEIGHT);
    layout->boxes[THEME_LAYOUT_MODES] = row_of(line, modes_top, SMALL_LINE_HEIGHT);
    layout->boxes[THEME_LAYOUT_PROGRESS] = row_of(line, progress_top, PROGRESS_HEIGHT);
    layout->boxes[THEME_LAYOUT_TIMES] =
        row_of(line, progress_top + PROGRESS_HEIGHT + TIMES_GAP, SMALL_LINE_HEIGHT);
}

static void landscape_layout(const NowPlayingScreen *now_playing, Layout *layout)
{
    const Theme *theme = now_playing->config.theme;
    int padding = theme->metrics.padding;
    int width = theme->metrics.screen_width;
    int left = text_left(now_playing);
    Rect column = {left, 0, width - left - padding, 0};
    Rect line = {padding, 0, width - 2 * padding, 0};
    layout->boxes[THEME_LAYOUT_COVER] =
        (Rect){padding, COVER_TOP, COVER_FULL_SIZE, COVER_FULL_SIZE};
    place_text(layout, column, TITLE_TOP, FORMAT_TOP);
    place_bottom(layout, column, MODES_TOP, PROGRESS_TOP);
    layout->boxes[THEME_LAYOUT_DETAIL].y = DETAIL_TOP;
    layout->boxes[THEME_LAYOUT_PROGRESS].x = line.x;
    layout->boxes[THEME_LAYOUT_PROGRESS].width = line.width;
    layout->boxes[THEME_LAYOUT_TIMES].x = line.x;
    layout->boxes[THEME_LAYOUT_TIMES].width = line.width;
    layout->boxes[THEME_LAYOUT_LYRICS] =
        row_of(column, LYRICS_TOP, LYRICS_ROWS * LYRICS_ROW_HEIGHT);
}

static void portrait_layout(const NowPlayingScreen *now_playing, Layout *layout)
{
    const Theme *theme = now_playing->config.theme;
    Image ignored;
    int padding = theme->metrics.padding;
    Rect line = {padding, 0, theme->metrics.screen_width - 2 * padding, 0};
    int cover_top = theme->metrics.header_height + PORTRAIT_COVER_MIN_GAP;
    cover_top = cover_top > PORTRAIT_COVER_TOP ? cover_top : PORTRAIT_COVER_TOP;
    int title_top = cover_top + COVER_FULL_SIZE + PORTRAIT_TEXT_GAP;
    if (!current_cover(now_playing, &ignored) && !lyrics_shown(now_playing)) {
        title_top = cover_top + (COVER_FULL_SIZE + PORTRAIT_TEXT_GAP) / 2;
    }
    int times_top = theme->metrics.screen_height - SMALL_LINE_HEIGHT - PORTRAIT_BOTTOM_MARGIN;
    int progress_top = times_top - TIMES_GAP - PROGRESS_HEIGHT;
    int lyrics_height = LYRICS_ROWS * LYRICS_ROW_HEIGHT;
    layout->boxes[THEME_LAYOUT_COVER] = (Rect){(theme->metrics.screen_width - COVER_FULL_SIZE) / 2,
                                               cover_top, COVER_FULL_SIZE, COVER_FULL_SIZE};
    place_text(layout, line, title_top, title_top + 3 * TEXT_LINE_HEIGHT + PORTRAIT_FORMAT_GAP);
    place_bottom(layout, line, progress_top - PORTRAIT_PROGRESS_GAP - SMALL_LINE_HEIGHT,
                 progress_top);
    layout->boxes[THEME_LAYOUT_LYRICS] =
        row_of(line, cover_top + (COVER_FULL_SIZE - lyrics_height) / 2, lyrics_height);
    for (int id = 0; id < THEME_LAYOUT_COUNT; id++) {
        layout->aligns[id] = THEME_ALIGN_CENTER;
    }
}

static void wear_layout(const ThemeLayout *worn, Layout *layout)
{
    for (int id = 0; worn != NULL && id < THEME_LAYOUT_COUNT; id++) {
        ThemeLayoutBox box = worn->boxes[id];
        if (box.align != THEME_ALIGN_UNSET) {
            int height = id == THEME_LAYOUT_COVER ? box.width : box.height;
            layout->boxes[id] = (Rect){box.x, box.y, box.width, height};
            layout->aligns[id] = box.align;
        }
    }
}

static void layout_of(const NowPlayingScreen *now_playing, Layout *layout)
{
    const Theme *theme = now_playing->config.theme;
    *layout = (Layout){.lyrics = lyrics_shown(now_playing)};
    if (theme_is_portrait(theme)) {
        portrait_layout(now_playing, layout);
    } else {
        landscape_layout(now_playing, layout);
    }
    wear_layout(theme->layout, layout);
}

void now_playing_builtin_layout(const NowPlayingScreen *now_playing, ThemeLayout *out)
{
    Layout layout;
    layout_of(now_playing, &layout);
    const ThemeLayout *worn = now_playing->config.theme->layout;
    for (int id = 0; id < THEME_LAYOUT_COUNT; id++) {
        Rect box = layout.boxes[id];
        out->boxes[id] = (ThemeLayoutBox){(int16_t)box.x, (int16_t)box.y, (uint16_t)box.width,
                                          (uint8_t)box.height, layout.aligns[id]};
    }
    if (worn != NULL) {
        Theme bare = *now_playing->config.theme;
        NowPlayingScreen plain = *now_playing;
        bare.layout = NULL;
        plain.config.theme = &bare;
        now_playing_builtin_layout(&plain, out);
    }
}

static bool shown(const Layout *layout, ThemeLayoutId id)
{
    Rect box = layout->boxes[id];
    bool spared = id == THEME_LAYOUT_TITLE || id >= THEME_LAYOUT_PROGRESS;
    Rect overlap = rect_intersection(box, layout->boxes[THEME_LAYOUT_LYRICS]);
    return box.width > 0 && (spared || !layout->lyrics || rect_is_empty(overlap));
}

static Rect fitted(uint8_t align, Rect area, int width)
{
    int room = area.width - width;
    if (align != THEME_ALIGN_LEFT && room > 0) {
        area.x += align == THEME_ALIGN_CENTER ? room / 2 : room;
        area.width = width;
    }
    return area;
}

static Label aligned(uint8_t align, const Font *font, Label label)
{
    label.area = fitted(align, label.area, text_width(font, label.text));
    return label;
}

static Line line_at(const Layout *layout, ThemeLayoutId id, const char *text, TextStyle style)
{
    return (Line){{layout->boxes[id], text}, style, layout->aligns[id]};
}

static void draw_cover(const NowPlayingScreen *now_playing, Canvas *canvas, const Layout *layout)
{
    Image cover;
    Rect box = layout->boxes[THEME_LAYOUT_COVER];
    if (!shown(layout, THEME_LAYOUT_COVER) || !current_cover(now_playing, &cover)) {
        return;
    }
    image_draw_quad(canvas, cover, (Quad){box.x, box.width, box.y, box.width, box.y, box.width});
}

static void draw_moving_line(const NowPlayingScreen *now_playing, Canvas *canvas, Line line,
                             uint32_t since_ms)
{
    int width = text_width(line.style.font, line.label.text);
    if (width <= line.label.area.width) {
        label_draw(canvas, line.style, aligned(line.align, line.style.font, line.label));
        return;
    }
    MarqueeTiming timing = now_playing->config.theme->marquee;
    uint32_t elapsed = now_playing->now_ms - since_ms;
    int offset = marquee_offset(timing, width, line.label.area.width, elapsed);
    label_draw_scrolling(canvas, line.style, line.label,
                         (Marquee){.offset = offset, .gap = timing.gap_px});
}

static void draw_lyrics(const NowPlayingScreen *now_playing, Canvas *canvas, const Layout *layout)
{
    const Theme *theme = now_playing->config.theme;
    const NowPlayingLyrics *lyrics = &now_playing->lyrics;
    Rect area = layout->boxes[THEME_LAYOUT_LYRICS];
    uint8_t align = layout->aligns[THEME_LAYOUT_LYRICS];
    bool synced = lyrics->lyrics.synced;
    int rows = area.height / LYRICS_ROW_HEIGHT < LYRICS_ROWS ? area.height / LYRICS_ROW_HEIGHT
                                                             : LYRICS_ROWS;
    for (int row = 0; row < rows && area.width > 0; row++) {
        int index = lyrics->current - rows / 2 + row;
        if (index < 0 || index >= lyrics->lyrics.count) {
            continue;
        }
        Label label = {{area.x, area.y + row * LYRICS_ROW_HEIGHT, area.width, LYRICS_ROW_HEIGHT},
                       lyrics_line(&lyrics->lyrics, index)};
        if (synced && index == lyrics->current) {
            TextStyle style = {.font = theme->fonts.title, .color = theme->colors->text};
            draw_moving_line(now_playing, canvas, (Line){label, style, align}, lyrics->current_ms);
            continue;
        }
        TextStyle style = {.font = theme->fonts.body,
                           .color = synced ? theme->colors->text_tertiary
                                           : theme->colors->text_secondary};
        label_draw(canvas, style, aligned(align, style.font, label));
    }
}

static void draw_track_text(const NowPlayingScreen *now_playing, Canvas *canvas,
                            const Layout *layout)
{
    const Theme *theme = now_playing->config.theme;
    const char *const texts[] = {now_playing->title, now_playing->artist, now_playing->album};
    const TextStyle styles[] = {
        {.font = theme->fonts.title, .color = theme->colors->text},
        {.font = theme->fonts.body, .color = theme->colors->text_secondary},
        {.font = theme->fonts.body, .color = theme->colors->text_tertiary},
    };
    for (int line = 0; line < 3; line++) {
        ThemeLayoutId id = (ThemeLayoutId)(THEME_LAYOUT_TITLE + line);
        if (shown(layout, id)) {
            draw_moving_line(now_playing, canvas, line_at(layout, id, texts[line], styles[line]),
                             now_playing->text_changed_ms);
        }
    }
}

static uint32_t shown_position(const NowPlayingScreen *now_playing)
{
    return now_playing->scrubbing ? now_playing->scrub_position_ms
                                  : now_playing->status.position_ms;
}

static void draw_progress(const NowPlayingScreen *now_playing, Canvas *canvas, const Layout *layout)
{
    const Theme *theme = now_playing->config.theme;
    Rect bar = layout->boxes[THEME_LAYOUT_PROGRESS];
    bool volume = showing(now_playing, NOW_PLAYING_FEEDBACK_VOLUME);
    int filled = volume ? now_playing_volume_width(now_playing->status.volume, bar.width)
                        : now_playing_progress_width(shown_position(now_playing),
                                                     now_playing->status.duration_ms, bar.width);
    const BoxStyle *fill = now_playing->scrubbing || volume ? &theme->boxes->progress_active
                                                            : &theme->boxes->progress_fill;
    box_draw(canvas, theme->colors, &theme->boxes->progress_track, bar);
    box_draw(canvas, theme->colors, fill, (Rect){bar.x, bar.y, filled, bar.height});
}

static void draw_right_aligned(Canvas *canvas, TextStyle style, Rect area, const char *text)
{
    int width = text_width(style.font, text);
    Rect right = {area.x + area.width - width, area.y, width, area.height};
    label_draw(canvas, style, (Label){.area = right, .text = text});
}

static void draw_times(const NowPlayingScreen *now_playing, Canvas *canvas, const Layout *layout)
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
    Rect area = layout->boxes[THEME_LAYOUT_TIMES];
    TextStyle style = {.font = theme->fonts.small, .color = theme->colors->text_secondary};
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

static void draw_format(const NowPlayingScreen *now_playing, Canvas *canvas, const Layout *layout)
{
    const Theme *theme = now_playing->config.theme;
    HalAudioFormat format = now_playing->status.format;
    const char *codec = now_playing_codec_name(format.codec);
    if (codec[0] == '\0' || !shown(layout, THEME_LAYOUT_FORMAT)) {
        return;
    }
    TextStyle style = {.font = theme->fonts.small, .color = theme->colors->text_secondary};
    int codec_width = text_width(style.font, codec) + 2 * CODEC_PADDING;
    char stream[STATUS_CAPACITY];
    now_playing_stream_text(format, stream, sizeof stream);
    int total = codec_width + CODEC_GAP + text_width(style.font, stream);
    Rect row =
        fitted(layout->aligns[THEME_LAYOUT_FORMAT], layout->boxes[THEME_LAYOUT_FORMAT], total);
    Rect badge = {row.x, row.y, codec_width, row.height};
    draw_outline(canvas, badge, theme->colors->text_secondary);
    Rect codec_text = label_centered_area(style.font, badge, codec);
    label_draw(canvas, style, (Label){.area = codec_text, .text = codec});
    int stream_left = badge.x + badge.width + CODEC_GAP;
    Rect stream_area = {stream_left, row.y, row.x + row.width - stream_left, row.height};
    label_draw(canvas, style, (Label){.area = stream_area, .text = stream});
}

static void draw_small_line(Canvas *canvas, Line line, const Layout *layout, ThemeLayoutId id)
{
    if (shown(layout, id)) {
        label_draw(canvas, line.style, aligned(line.align, line.style.font, line.label));
    }
}

static void draw_details(const NowPlayingScreen *now_playing, Canvas *canvas, const Layout *layout)
{
    const Theme *theme = now_playing->config.theme;
    TextStyle style = {.font = theme->fonts.small, .color = theme->colors->text_tertiary};
    char detail[STATUS_CAPACITY];
    now_playing_detail_text(&now_playing->status, detail, sizeof detail);
    draw_small_line(canvas, line_at(layout, THEME_LAYOUT_DETAIL, detail, style), layout,
                    THEME_LAYOUT_DETAIL);
    char modes[STATUS_CAPACITY];
    modes_text(&now_playing->status, modes, sizeof modes);
    if (now_playing->scrubbing) {
        StringBuilder seeking = string_builder_make(modes, sizeof modes);
        string_builder_append(&seeking, i18n_text(STR_MENU_SEEKING_HINT));
    }
    draw_small_line(canvas, line_at(layout, THEME_LAYOUT_MODES, modes, style), layout,
                    THEME_LAYOUT_MODES);
}

static void draw_badge_icon(Canvas *canvas, const Theme *theme, NowPlayingFeedback feedback,
                            Rect area)
{
    Icon icon = {.area = area, .color = theme->colors->text};
    switch (feedback) {
    case NOW_PLAYING_FEEDBACK_NEXT:
        icon.id = THEME_ICON_BADGE_NEXT;
        break;
    case NOW_PLAYING_FEEDBACK_PREVIOUS:
        icon.id = THEME_ICON_BADGE_PREVIOUS;
        break;
    case NOW_PLAYING_FEEDBACK_PAUSE:
        icon.id = THEME_ICON_BADGE_PAUSE;
        break;
    case NOW_PLAYING_FEEDBACK_PLAY:
        icon.id = THEME_ICON_BADGE_PLAY;
        break;
    default:
        return;
    }
    icon_draw(canvas, theme, icon);
}

static void draw_badge(const NowPlayingScreen *now_playing, Canvas *canvas, const Layout *layout)
{
    Image cover;
    NowPlayingFeedback feedback = now_playing->feedback;
    if (feedback == NOW_PLAYING_FEEDBACK_VOLUME || !showing(now_playing, feedback) ||
        !current_cover(now_playing, &cover) || !shown(layout, THEME_LAYOUT_COVER)) {
        return;
    }
    const Theme *theme = now_playing->config.theme;
    Rect box = layout->boxes[THEME_LAYOUT_COVER];
    int center_x = box.x + box.width / 2;
    int center_y = box.y + box.width / 2;
    Rect badge = {center_x - BADGE_SIZE / 2, center_y - BADGE_SIZE / 2, BADGE_SIZE, BADGE_SIZE};
    Rect icon = {center_x - BADGE_ICON_SIZE / 2, center_y - BADGE_ICON_SIZE / 2, BADGE_ICON_SIZE,
                 BADGE_ICON_SIZE};
    canvas_fill_rect(canvas, badge, theme->colors->background);
    draw_badge_icon(canvas, theme, feedback, icon);
}

static void draw_nothing_playing(const Theme *theme, Canvas *canvas)
{
    TextStyle style = {.font = theme->fonts.body, .color = theme->colors->text_secondary};
    Rect content = theme_content_area(theme);
    const char *message = i18n_text(STR_MENU_NOTHING_PLAYING);
    Rect area = label_centered_area(style.font, content, message);
    label_draw(canvas, style, (Label){.area = area, .text = message});
}

static const NowPlayingFaces *face_code(const NowPlayingScreen *now_playing)
{
    if (!now_playing->status.has_track || !theme_store_has_scene(THEME_SCENE_NOW_PLAYING)) {
        return NULL;
    }
    return hal_system_load_faces();
}

static const NowPlayingFaces *scene_of(const NowPlayingScreen *now_playing, SceneCode *scene,
                                       const ThemeColors **chrome)
{
    const NowPlayingFaces *faces = face_code(now_playing);
    *chrome = NULL;
    *scene = (SceneCode){0};
    return faces != NULL && theme_store_scene(THEME_SCENE_NOW_PLAYING, scene, chrome) ? faces
                                                                                      : NULL;
}

const ThemeColors *now_playing_face_colors(const NowPlayingScreen *now_playing)
{
    SceneCode scene;
    const ThemeColors *chrome = NULL;
    return scene_of(now_playing, &scene, &chrome) != NULL ? chrome : NULL;
}

static NowPlayingFeedback badge_of(const NowPlayingScreen *now_playing)
{
    NowPlayingFeedback feedback = now_playing->feedback;
    bool shown = feedback != NOW_PLAYING_FEEDBACK_VOLUME && showing(now_playing, feedback);
    return shown ? feedback : NOW_PLAYING_FEEDBACK_NONE;
}

static void face_texts(const NowPlayingScreen *now_playing, NowPlayingFrame *frame, char *texts)
{
    char *detail = texts + FACE_STREAM_CAPACITY;
    char *modes = detail + FACE_DETAIL_CAPACITY;
    now_playing_stream_text(now_playing->status.format, texts, FACE_STREAM_CAPACITY);
    now_playing_detail_text(&now_playing->status, detail, FACE_DETAIL_CAPACITY);
    modes_text(&now_playing->status, modes, FACE_MODES_CAPACITY);
    frame->stream = texts;
    frame->detail = detail;
    frame->modes = modes;
}

static void frame_state(const NowPlayingScreen *now_playing, NowPlayingFrame *frame)
{
    frame->now_ms = now_playing->now_ms;
    frame->text_ms = now_playing->text_changed_ms;
    frame->lyric_ms = now_playing->lyrics.current_ms;
    frame->shown_ms = shown_position(now_playing);
    frame->tick = now_playing->status.position_ms / NP_FACE_TICK_MS;
    frame->feedback = badge_of(now_playing);
    frame->playing = now_playing->status.state == PLAYER_PLAYING;
    frame->scrubbing = now_playing->scrubbing;
    frame->volume_shown = showing(now_playing, NOW_PLAYING_FEEDBACK_VOLUME);
}

static void frame_of(const NowPlayingScreen *now_playing, NowPlayingFrame *frame, char *texts)
{
    const Theme *theme = now_playing->config.theme;
    frame->theme = theme;
    frame->status = &now_playing->status;
    frame->song = now_playing->title;
    frame->artist = now_playing->artist;
    frame->album = now_playing->album;
    frame->lyric = now_playing->lyrics.lyrics.synced ? now_playing_lyric(now_playing) : NULL;
    frame->codec = now_playing_codec_name(now_playing->status.format.codec);
    frame->area = theme_content_area(theme);
    frame_state(now_playing, frame);
    frame->cover = (Image){0};
    current_cover(now_playing, &frame->cover);
    frame->menu = NULL;
    face_texts(now_playing, frame, texts);
}

static void request_tick(const NowPlayingScreen *now_playing)
{
    const PlayerStatus *status = &now_playing->status;
    if (status->state == PLAYER_PLAYING) {
        marquee_frame_request(NP_FACE_TICK_MS - status->position_ms % NP_FACE_TICK_MS);
    }
}

static void home_frame(const NowPlayingScreen *now_playing, const NowPlayingHome *home,
                       NowPlayingFrame *frame)
{
    frame->menu = home->menu;
    frame->cover = home->cover;
    if (!now_playing->status.has_track) {
        frame->song = NULL;
        frame->artist = NULL;
        frame->album = NULL;
    }
}

static void draw_face(const NowPlayingScreen *now_playing, const NowPlayingFaces *faces,
                      SceneCode scene, const ThemeColors *chrome, Canvas *canvas,
                      const NowPlayingHome *home)
{
    char texts[FACE_TEXTS_CAPACITY];
    Theme dressed = *now_playing->config.theme;
    dressed.colors = chrome != NULL ? chrome : dressed.colors;
    header_draw(canvas, &dressed, home != NULL ? home->title : now_playing->screen.title);
    NowPlayingFrame frame;
    frame_of(now_playing, &frame, texts);
    frame.scene = scene;
    if (home != NULL) {
        home_frame(now_playing, home, &frame);
    }
    request_tick(now_playing);
    faces->draw(&frame, canvas);
}

void now_playing_follow(NowPlayingScreen *now_playing, uint32_t now_ms)
{
    refresh(now_playing, now_ms);
}

static const NowPlayingFaces *home_scene(SceneCode *scene, const ThemeColors **chrome)
{
    const NowPlayingFaces *faces =
        theme_store_has_scene(THEME_SCENE_HOME) ? hal_system_load_faces() : NULL;
    *chrome = NULL;
    return faces != NULL && theme_store_scene(THEME_SCENE_HOME, scene, chrome) ? faces : NULL;
}

bool now_playing_draw_home(const NowPlayingScreen *now_playing, const NowPlayingHome *home,
                           Canvas *canvas)
{
    SceneCode scene;
    const ThemeColors *chrome;
    const NowPlayingFaces *faces = home_scene(&scene, &chrome);
    if (faces == NULL) {
        return false;
    }
    draw_face(now_playing, faces, scene, chrome, canvas, home);
    return true;
}

const ThemeColors *now_playing_home_colors(void)
{
    SceneCode scene;
    const ThemeColors *chrome;
    return home_scene(&scene, &chrome) != NULL ? chrome : NULL;
}

static void draw(void *context, Canvas *canvas)
{
    const NowPlayingScreen *now_playing = context;
    SceneCode scene;
    const ThemeColors *chrome;
    const NowPlayingFaces *faces = scene_of(now_playing, &scene, &chrome);
    if (faces != NULL) {
        draw_face(now_playing, faces, scene, chrome, canvas, NULL);
        return;
    }
    const Theme *theme = now_playing->config.theme;
    backdrop_paint(canvas, theme, theme_content_area(theme));
    header_draw(canvas, theme, now_playing->screen.title);
    if (!now_playing->status.has_track) {
        draw_nothing_playing(theme, canvas);
        return;
    }
    Layout layout;
    layout_of(now_playing, &layout);
    draw_cover(now_playing, canvas, &layout);
    draw_badge(now_playing, canvas, &layout);
    draw_track_text(now_playing, canvas, &layout);
    draw_progress(now_playing, canvas, &layout);
    draw_times(now_playing, canvas, &layout);
    if (layout.lyrics) {
        draw_lyrics(now_playing, canvas, &layout);
    }
    draw_format(now_playing, canvas, &layout);
    draw_details(now_playing, canvas, &layout);
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

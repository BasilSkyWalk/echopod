#include "face_text.h"

#include "core/format.h"
#include "core/i18n.h"
#include "core/resident.h"
#include "core/string_builder.h"
#include "ui/label.h"

enum {
    MILLISECONDS_PER_SECOND = 1000,
    BADGE_ICON_COUNT = 6,
    PAUSE_BAR_PERCENT = 30,
    SKIP_BAR_DIVISOR = 6,
    HALF = 2
};

ECHO_NP_FACES_DATA static const uint8_t BADGE_ICONS[BADGE_ICON_COUNT] = {
    [NOW_PLAYING_FEEDBACK_NONE] = FACE_ICON_NONE,
    [NOW_PLAYING_FEEDBACK_VOLUME] = FACE_ICON_NONE,
    [NOW_PLAYING_FEEDBACK_NEXT] = FACE_ICON_NEXT,
    [NOW_PLAYING_FEEDBACK_PREVIOUS] = FACE_ICON_PREVIOUS,
    [NOW_PLAYING_FEEDBACK_PAUSE] = FACE_ICON_PAUSE,
    [NOW_PLAYING_FEEDBACK_PLAY] = FACE_ICON_PLAY,
};

ECHO_NP_FACES_CODE static int larger(int first, int second)
{
    return first > second ? first : second;
}

ECHO_NP_FACES_CODE static int smaller(int first, int second)
{
    return first < second ? first : second;
}

ECHO_NP_FACES_CODE static bool has_text(const char *text)
{
    return text != NULL && text[0] != '\0';
}

ECHO_NP_FACES_CODE int face_scroll_offset(MarqueeTiming timing, int text_width, int view_width,
                                          uint32_t elapsed_ms)
{
    if (text_width <= view_width || timing.speed_px_per_s == 0) {
        return 0;
    }
    uint32_t cycle = timing.pause_ms + (uint32_t)(text_width + timing.gap_px) *
                                           MILLISECONDS_PER_SECOND / timing.speed_px_per_s;
    uint32_t phase = (elapsed_ms - elapsed_ms % NP_FACE_TICK_MS) % cycle;
    if (phase < timing.pause_ms) {
        return 0;
    }
    return (int)((phase - timing.pause_ms) * timing.speed_px_per_s / MILLISECONDS_PER_SECOND);
}

ECHO_NP_FACES_CODE Rect face_fitted_area(const Font *font, Rect area, const char *text,
                                         FaceAlign align)
{
    int width = smaller(text_width(font, text), area.width);
    if (align == FACE_ALIGN_CENTRE) {
        return (Rect){area.x + (area.width - width) / HALF, area.y, width, area.height};
    }
    if (align == FACE_ALIGN_RIGHT) {
        return (Rect){area.x + area.width - width, area.y, width, area.height};
    }
    return area;
}

ECHO_NP_FACES_CODE static void scroll_draw(Canvas *canvas, const NowPlayingFrame *frame,
                                           TextStyle style, Label label)
{
    MarqueeTiming timing = frame->theme->marquee;
    int width = text_width(style.font, label.text);
    uint32_t since = label.text == frame->lyric ? frame->lyric_ms : frame->text_ms;
    int offset = face_scroll_offset(timing, width, label.area.width, frame->now_ms - since);
    label_draw_scrolling(canvas, style, label, (Marquee){offset, timing.gap_px});
}

ECHO_NP_FACES_CODE void face_line(Canvas *canvas, const NowPlayingFrame *frame,
                                  const FaceLine *line, const char *text)
{
    if (text == NULL) {
        return;
    }
    TextStyle style = {line->font, line->color};
    if (text_width(line->font, text) <= line->area.width) {
        Rect fitted = face_fitted_area(line->font, line->area, text, line->align);
        label_draw(canvas, style, (Label){fitted, text});
    } else if (frame->playing && !frame->scrubbing) {
        scroll_draw(canvas, frame, style, (Label){line->area, text});
    } else {
        label_draw(canvas, style, (Label){line->area, text});
    }
}

ECHO_NP_FACES_CODE const char *face_feedback_text(const NowPlayingFrame *frame, char *buffer,
                                                  size_t capacity)
{
    if (frame->scrubbing) {
        return i18n_text(STR_MENU_SEEKING_HINT);
    }
    if (!frame->volume_shown) {
        return NULL;
    }
    StringBuilder builder = string_builder_make(buffer, capacity);
    string_builder_append(&builder, i18n_text(STR_MENU_VOLUME));
    string_builder_append_char(&builder, ' ');
    string_builder_append_unsigned(&builder, frame->status->volume);
    return buffer;
}

ECHO_NP_FACES_CODE void face_feedback_line(Canvas *canvas, const NowPlayingFrame *frame,
                                           const FaceLine *line, const char *text)
{
    char buffer[FACE_FEEDBACK_CAPACITY];
    const char *feedback = face_feedback_text(frame, buffer, sizeof buffer);
    FaceLine emphasised = *line;
    emphasised.color = line->emphasis;
    if (feedback != NULL) {
        Rect fitted = face_fitted_area(line->font, line->area, feedback, line->align);
        label_draw(canvas, (TextStyle){line->font, line->emphasis}, (Label){fitted, feedback});
    } else if (has_text(frame->lyric)) {
        face_line(canvas, frame, &emphasised, frame->lyric);
    } else {
        face_line(canvas, frame, line, text);
    }
}

ECHO_NP_FACES_CODE void face_times_text(const NowPlayingFrame *frame, FaceTimes *times)
{
    uint32_t shown = frame->shown_ms;
    uint32_t duration = frame->status->duration_ms;
    format_duration(shown, times->elapsed, sizeof times->elapsed);
    times->remaining[0] = '-';
    format_duration(duration > shown ? duration - shown : 0, times->remaining + 1,
                    sizeof times->remaining - 1);
}

ECHO_NP_FACES_CODE void face_times_draw(Canvas *canvas, const NowPlayingFrame *frame,
                                        const FaceLine *line)
{
    FaceTimes times;
    face_times_text(frame, &times);
    label_draw(canvas, (TextStyle){line->font, line->emphasis}, (Label){line->area, times.elapsed});
    Rect right = face_fitted_area(line->font, line->area, times.remaining, FACE_ALIGN_RIGHT);
    label_draw(canvas, (TextStyle){line->font, line->color}, (Label){right, times.remaining});
}

ECHO_NP_FACES_CODE int face_progress_width(uint32_t position_ms, uint32_t duration_ms, int width)
{
    if (duration_ms == 0) {
        return 0;
    }
    if (position_ms >= duration_ms) {
        return width;
    }
    return (int)((uint64_t)position_ms * (uint64_t)width / duration_ms);
}

ECHO_NP_FACES_CODE int face_volume_width(uint8_t volume, int width)
{
    int level = volume < SETTINGS_VOLUME_MAX ? volume : SETTINGS_VOLUME_MAX;
    return level * width / SETTINGS_VOLUME_MAX;
}

ECHO_NP_FACES_CODE FaceFill face_bar_fill(const NowPlayingFrame *frame, int width)
{
    if (frame->volume_shown) {
        return (FaceFill){face_volume_width(frame->status->volume, width), true};
    }
    return (FaceFill){face_progress_width(frame->shown_ms, frame->status->duration_ms, width),
                      frame->scrubbing};
}

ECHO_NP_FACES_CODE void face_bar_draw(Canvas *canvas, const NowPlayingFrame *frame,
                                      const FaceBar *bar)
{
    FaceFill fill = face_bar_fill(frame, bar->area.width);
    canvas_fill_rect(canvas, bar->area, bar->track);
    canvas_fill_rect(canvas, (Rect){bar->area.x, bar->area.y, fill.width, bar->area.height},
                     fill.hot ? bar->hot : bar->normal);
}

ECHO_NP_FACES_CODE FaceIcon face_badge_kind(const NowPlayingFrame *frame)
{
    return (unsigned)frame->feedback < BADGE_ICON_COUNT ? (FaceIcon)BADGE_ICONS[frame->feedback]
                                                        : FACE_ICON_NONE;
}

ECHO_NP_FACES_CODE static void triangle(Canvas *canvas, Rect box, Color color, bool points_right)
{
    for (int row = 0; row < box.height; row++) {
        int reach = row < box.height / HALF ? row : box.height - 1 - row;
        int width = smaller(HALF * reach + 1, box.width);
        int left = points_right ? box.x : box.x + box.width - width;
        canvas_fill_rect(canvas, (Rect){left, box.y + row, width, 1}, color);
    }
}

ECHO_NP_FACES_CODE static void pause_icon(Canvas *canvas, Rect box, Color color)
{
    int bar = larger(box.width * PAUSE_BAR_PERCENT / 100, 1);
    canvas_fill_rect(canvas, (Rect){box.x, box.y, bar, box.height}, color);
    canvas_fill_rect(canvas, (Rect){box.x + box.width - bar, box.y, bar, box.height}, color);
}

ECHO_NP_FACES_CODE static void next_icon(Canvas *canvas, Rect box, Color color)
{
    int bar = larger(box.width / SKIP_BAR_DIVISOR, 1);
    int half = (box.width - bar) / HALF;
    triangle(canvas, (Rect){box.x, box.y, half, box.height}, color, true);
    triangle(canvas, (Rect){box.x + half, box.y, half, box.height}, color, true);
    canvas_fill_rect(canvas, (Rect){box.x + box.width - bar, box.y, bar, box.height}, color);
}

ECHO_NP_FACES_CODE static void previous_icon(Canvas *canvas, Rect box, Color color)
{
    int bar = larger(box.width / SKIP_BAR_DIVISOR, 1);
    int half = (box.width - bar) / HALF;
    canvas_fill_rect(canvas, (Rect){box.x, box.y, bar, box.height}, color);
    triangle(canvas, (Rect){box.x + bar, box.y, half, box.height}, color, false);
    triangle(canvas, (Rect){box.x + bar + half, box.y, half, box.height}, color, false);
}

ECHO_NP_FACES_CODE void face_icon_draw(Canvas *canvas, FaceIcon icon, Rect box, Color color)
{
    switch (icon) {
    case FACE_ICON_PLAY:
        triangle(canvas, box, color, true);
        break;
    case FACE_ICON_PAUSE:
        pause_icon(canvas, box, color);
        break;
    case FACE_ICON_NEXT:
        next_icon(canvas, box, color);
        break;
    case FACE_ICON_PREVIOUS:
        previous_icon(canvas, box, color);
        break;
    case FACE_ICON_NONE:
        break;
    }
}

ECHO_NP_FACES_CODE void face_badge_draw(Canvas *canvas, const NowPlayingFrame *frame, Rect box,
                                        Color color)
{
    face_icon_draw(canvas, face_badge_kind(frame), box, color);
}

ECHO_NP_FACES_CODE void face_outline(Canvas *canvas, Rect box, Color color)
{
    canvas_fill_rect(canvas, (Rect){box.x, box.y, box.width, 1}, color);
    canvas_fill_rect(canvas, (Rect){box.x, box.y + box.height - 1, box.width, 1}, color);
    canvas_fill_rect(canvas, (Rect){box.x, box.y, 1, box.height}, color);
    canvas_fill_rect(canvas, (Rect){box.x + box.width - 1, box.y, 1, box.height}, color);
}

ECHO_NP_FACES_CODE int face_codec_width(const Font *font, const char *codec)
{
    return has_text(codec) ? text_width(font, codec) + FACE_CODEC_PADDING : 0;
}

ECHO_NP_FACES_CODE void face_codec_draw(Canvas *canvas, const FaceLine *line, const char *codec)
{
    if (!has_text(codec)) {
        return;
    }
    face_outline(canvas, line->area, line->color);
    Rect fitted = face_fitted_area(line->font, line->area, codec, FACE_ALIGN_CENTRE);
    label_draw(canvas, (TextStyle){line->font, line->color}, (Label){fitted, codec});
}

ECHO_NP_FACES_CODE static int format_origin(const FaceLine *line, int box_width, int stream_width)
{
    int total = box_width + (box_width > 0 ? FACE_FORMAT_GAP : 0) + stream_width;
    if (line->align == FACE_ALIGN_CENTRE && total < line->area.width) {
        return line->area.x + (line->area.width - total) / HALF;
    }
    return line->area.x;
}

ECHO_NP_FACES_CODE void face_format_row(Canvas *canvas, const NowPlayingFrame *frame,
                                        const FaceLine *line)
{
    const char *stream = frame->stream;
    int box_width = face_codec_width(line->font, frame->codec);
    int stream_width = has_text(stream) ? text_width(line->font, stream) : 0;
    int x = format_origin(line, box_width, stream_width);
    if (box_width > 0) {
        Rect box = {x, line->area.y, box_width, line->area.height};
        FaceLine boxed = {line->font, box, line->emphasis, line->color, FACE_ALIGN_LEFT};
        face_codec_draw(canvas, &boxed, frame->codec);
        x += box_width + FACE_FORMAT_GAP;
    }
    if (has_text(stream)) {
        Rect rest = {x, line->area.y, line->area.x + line->area.width - x, line->area.height};
        label_draw(canvas, (TextStyle){line->font, line->color}, (Label){rest, stream});
    }
}

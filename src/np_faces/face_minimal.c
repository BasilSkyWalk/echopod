#include "core/format.h"
#include "core/i18n.h"
#include "core/resident.h"
#include "core/string_builder.h"
#include "core/utf8.h"
#include "face_text.h"
#include "faces.h"
#include "gfx/image.h"
#include "gfx/text.h"
#include "ui/label.h"

enum {
    COVER_SIDE = 170,
    COVER_DRAW_SIDE = 168,
    LINE_BIG = 20,
    LINE_SMALL = 14,
    BADGE_SIDE = 40,
    BADGE_ICON_SIDE = 18,
    PROGRESS_HEIGHT = 3,
    QUEUE_CAPACITY = 48,
    TIMES_CAPACITY = 32,
    BITRATE_CAPACITY = 24,
    DIGITS_CAPACITY = 8
};

typedef struct MinimalRows {
    int x;
    int width;
    int title;
    int title_next;
    int artist;
    int album;
    int modes;
    int queue;
    int bottom;
    int bar;
    FaceAlign small_align;
} MinimalRows;

typedef struct MinimalWrap {
    size_t first_bytes;
    bool first_ellipsis;
    const char *second;
} MinimalWrap;

ECHO_NP_FACES_DATA static const char SEPARATOR[] = " \xC2\xB7 ";
ECHO_NP_FACES_DATA static const char TIME_SEPARATOR[] = " · ";
ECHO_NP_FACES_DATA static const char TIME_SAMPLE[] = "0:00";
ECHO_NP_FACES_DATA static const char UNKNOWN_ARTIST[] = "Unknown Artist";

ECHO_NP_FACES_DATA static const MinimalRows LANDSCAPE = {
    178, 134, 24, 46, 68, 90, 117, 133, 149, 167, FACE_ALIGN_LEFT};

ECHO_NP_FACES_DATA static const MinimalRows PORTRAIT = {
    8, 154, 176, 198, 222, 244, 268, 284, 300, 317, FACE_ALIGN_LEFT};

ECHO_NP_FACES_CODE static const MinimalRows *rows_of(const NowPlayingFrame *frame)
{
    return frame->area.height > frame->area.width ? &PORTRAIT : &LANDSCAPE;
}

ECHO_NP_FACES_CODE static Color background_color(const Theme *theme)
{
    return theme->colors.background == COLOR_RGB(16, 17, 20) ? COLOR_RGB(0, 0, 0)
                                                             : theme->colors.background;
}

ECHO_NP_FACES_CODE static Rect column_area(const NowPlayingFrame *frame, const MinimalRows *rows,
                                           int top, int height)
{
    int x = rows->x;
    int width = rows->width;
    int available = frame->area.width - x;
    width = width < available ? width : available;
    width = width > 0 ? width : 0;
    return (Rect){frame->area.x + x, frame->area.y + top, width, height};
}

ECHO_NP_FACES_CODE static void cover_draw(const NowPlayingFrame *frame, Canvas *canvas)
{
    Image cover = frame->cover;
    Rect area = frame->area;
    if (cover.pixels == NULL) {
        return;
    }
    image_draw_quad(
        canvas, cover,
        (Quad){area.x, COVER_DRAW_SIDE, area.y, COVER_DRAW_SIDE, area.y, COVER_DRAW_SIDE});
}

ECHO_NP_FACES_CODE static void badge_draw(const NowPlayingFrame *frame, Canvas *canvas,
                                          Color background)
{
    if (face_badge_kind(frame) == FACE_ICON_NONE) {
        return;
    }
    int centre_x = frame->area.x + COVER_SIDE / 2;
    int centre_y = frame->area.y + COVER_SIDE / 2;
    Rect badge = {centre_x - BADGE_SIDE / 2, centre_y - BADGE_SIDE / 2, BADGE_SIDE, BADGE_SIDE};
    Rect icon = {centre_x - BADGE_ICON_SIDE / 2, centre_y - BADGE_ICON_SIDE / 2, BADGE_ICON_SIDE,
                 BADGE_ICON_SIDE};
    canvas_fill_rect(canvas, badge, background);
    face_badge_draw(canvas, frame, icon, frame->theme->colors.text);
}

ECHO_NP_FACES_CODE static void span_draw(Canvas *canvas, TextStyle style, Rect area, TextSpan span)
{
    Canvas clipped = canvas_clipped(canvas, area);
    if (rect_is_empty(clipped.clip)) {
        return;
    }
    Point pen = {area.x, label_baseline(style.font, area)};
    text_draw_span(&clipped, style, pen, span);
}

ECHO_NP_FACES_CODE static int glyph_advance(const Font *font, uint32_t codepoint)
{
    const Glyph *glyph = font_find_glyph(font, codepoint);
    return glyph != NULL ? glyph->advance : 0;
}

ECHO_NP_FACES_CODE static size_t text_length(const char *text)
{
    size_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

ECHO_NP_FACES_CODE static bool text_equals(const char *text, const char *expected)
{
    size_t index = 0;
    while (text[index] != '\0' && text[index] == expected[index]) {
        index++;
    }
    return text[index] == expected[index];
}

ECHO_NP_FACES_CODE static MinimalWrap wrap_title(const Font *font, const char *title, int width)
{
    MinimalWrap wrap = {text_length(title), false, NULL};
    if (text_width(font, title) <= width) {
        return wrap;
    }
    const char *cursor = title;
    const char *space = NULL;
    int used = 0;
    for (uint32_t codepoint = utf8_next(&cursor); codepoint != 0; codepoint = utf8_next(&cursor)) {
        used += glyph_advance(font, codepoint);
        if (used > width) {
            break;
        }
        if (codepoint == ' ') {
            space = cursor - 1;
        }
    }
    if (space != NULL && space > title) {
        const char *second = space + 1;
        while (*second == ' ') {
            second++;
        }
        wrap.first_bytes = (size_t)(space - title);
        wrap.second = *second != '\0' ? second : NULL;
        return wrap;
    }
    wrap.first_bytes = 0;
    wrap.first_ellipsis = true;
    return wrap;
}

ECHO_NP_FACES_CODE static void title_draw(const NowPlayingFrame *frame, Canvas *canvas)
{
    const char *title = frame->song;
    if (title == NULL || title[0] == '\0') {
        return;
    }
    const MinimalRows *rows = rows_of(frame);
    TextStyle style = {frame->theme->fonts.title, frame->theme->colors.text};
    Rect first = column_area(frame, rows, rows->title, LINE_BIG);
    MinimalWrap wrap = wrap_title(style.font, title, first.width);
    if (wrap.first_ellipsis) {
        label_draw(canvas, style, (Label){first, title});
    } else {
        span_draw(canvas, style, first, (TextSpan){title, wrap.first_bytes});
    }
    if (wrap.second != NULL) {
        Rect second = column_area(frame, rows, rows->title_next, LINE_BIG);
        label_draw(canvas, style, (Label){second, wrap.second});
    }
}

ECHO_NP_FACES_CODE static void credits_draw(const NowPlayingFrame *frame, Canvas *canvas)
{
    const MinimalRows *rows = rows_of(frame);
    const Theme *theme = frame->theme;
    if (frame->artist != NULL && !text_equals(frame->artist, UNKNOWN_ARTIST)) {
        Rect area = column_area(frame, rows, rows->artist, LINE_BIG);
        label_draw(canvas, (TextStyle){theme->fonts.body, theme->colors.text_secondary},
                   (Label){area, frame->artist});
    }
    Rect album = column_area(frame, rows, rows->album, LINE_BIG);
    label_draw(canvas, (TextStyle){theme->fonts.body, theme->colors.text_tertiary},
               (Label){album, frame->album});
}

ECHO_NP_FACES_CODE static void minimal_modes_text(const NowPlayingFrame *frame, char *buffer,
                                                  size_t capacity)
{
    StringBuilder builder = string_builder_make(buffer, capacity);
    const PlayerStatus *status = frame->status;
    if (frame->scrubbing) {
        string_builder_append(&builder, i18n_text(STR_MENU_SEEKING_HINT));
        return;
    }
    if (status->shuffle != SHUFFLE_OFF) {
        string_builder_append(&builder, i18n_text(status->shuffle == SHUFFLE_SONGS
                                                      ? STR_SETTINGS_SHUFFLE
                                                      : STR_MENU_SHUFFLE_ALBUMS));
    }
    if (status->shuffle != SHUFFLE_OFF && status->repeat != REPEAT_OFF) {
        string_builder_append(&builder, SEPARATOR);
    }
    if (status->repeat == REPEAT_ONE) {
        string_builder_append(&builder, i18n_text(STR_MENU_REPEAT_ONE));
    } else if (status->repeat == REPEAT_ALL) {
        string_builder_append(&builder, i18n_text(STR_MENU_REPEAT_ALL));
    }
}

ECHO_NP_FACES_CODE static void modes_draw(const NowPlayingFrame *frame, Canvas *canvas)
{
    char buffer[FACE_FEEDBACK_CAPACITY];
    minimal_modes_text(frame, buffer, sizeof buffer);
    if (buffer[0] == '\0') {
        return;
    }
    const MinimalRows *rows = rows_of(frame);
    TextStyle style = {frame->theme->fonts.small, frame->theme->colors.text_tertiary};
    Rect area = column_area(frame, rows, rows->modes, LINE_SMALL);
    label_draw(canvas, style,
               (Label){face_fitted_area(style.font, area, buffer, rows->small_align), buffer});
}

ECHO_NP_FACES_CODE static void queue_text(const NowPlayingFrame *frame, char *buffer,
                                          size_t capacity)
{
    buffer[0] = '\0';
    PlayQueuePlace place = frame->status->place;
    if (place.total == 0) {
        return;
    }
    char number[DIGITS_CAPACITY];
    char total[DIGITS_CAPACITY];
    StringBuilder number_builder = string_builder_make(number, sizeof number);
    StringBuilder total_builder = string_builder_make(total, sizeof total);
    string_builder_append_unsigned(&number_builder, place.number);
    string_builder_append_unsigned(&total_builder, place.total);
    const char *arguments[] = {number, total};
    StringBuilder builder = string_builder_make(buffer, capacity);
    i18n_format(&builder, STR_MENU_POSITION_OF, arguments, 2);
}

ECHO_NP_FACES_CODE static void queue_draw(const NowPlayingFrame *frame, Canvas *canvas)
{
    char buffer[QUEUE_CAPACITY];
    queue_text(frame, buffer, sizeof buffer);
    if (buffer[0] == '\0') {
        return;
    }
    const MinimalRows *rows = rows_of(frame);
    TextStyle style = {frame->theme->fonts.small, frame->theme->colors.text_tertiary};
    Rect area = column_area(frame, rows, rows->queue, LINE_SMALL);
    label_draw(canvas, style,
               (Label){face_fitted_area(style.font, area, buffer, rows->small_align), buffer});
}

ECHO_NP_FACES_CODE static void times_draw(const NowPlayingFrame *frame, Canvas *canvas, Rect area,
                                          TextStyle style)
{
    char elapsed[FACE_TIME_CAPACITY];
    char duration[FACE_TIME_CAPACITY];
    format_duration(frame->shown_ms, elapsed, sizeof elapsed);
    format_duration(frame->status->duration_ms, duration, sizeof duration);
    int elapsed_width = text_width(style.font, TIME_SAMPLE);
    elapsed_width = elapsed_width < area.width ? elapsed_width : area.width;
    int separator_width = text_width(style.font, TIME_SEPARATOR);
    separator_width =
        separator_width < area.width - elapsed_width ? separator_width : area.width - elapsed_width;
    int duration_width = area.width - elapsed_width - separator_width;
    Rect separator = {area.x + elapsed_width, area.y, separator_width, LINE_SMALL};
    Rect remaining = {separator.x + separator_width, area.y, duration_width, LINE_SMALL};
    label_draw(canvas, style, (Label){(Rect){area.x, area.y, elapsed_width, LINE_SMALL}, elapsed});
    label_draw(canvas, style, (Label){separator, TIME_SEPARATOR});
    label_draw(canvas, style, (Label){remaining, duration});
}

ECHO_NP_FACES_CODE static void bottom_draw(const NowPlayingFrame *frame, Canvas *canvas)
{
    const MinimalRows *rows = rows_of(frame);
    const Font *font = frame->theme->fonts.small;
    Rect area = column_area(frame, rows, rows->bottom, LINE_SMALL);
    TextStyle style = {font, frame->theme->colors.text_secondary};
    char buffer[TIMES_CAPACITY];
    const char *text = NULL;
    if (frame->volume_shown) {
        text = face_feedback_text(frame, buffer, sizeof buffer);
    }
    if (text != NULL) {
        int width = text_width(font, text);
        width = width < area.width ? width : area.width;
        label_draw(canvas, style, (Label){(Rect){area.x, area.y, width, LINE_SMALL}, text});
    } else {
        times_draw(frame, canvas, area, style);
    }
    int codec_width = face_codec_width(font, frame->codec);
    if (codec_width > 0) {
        Color codec_color = frame->theme->colors.text_tertiary;
        FaceLine line = {font,
                         {area.x + area.width - codec_width, area.y - 1, codec_width, LINE_SMALL},
                         codec_color,
                         codec_color,
                         FACE_ALIGN_RIGHT};
        face_codec_draw(canvas, &line, frame->codec);
        uint32_t bitrate = frame->status->format.bitrate_kbps;
        if (bitrate > 0) {
            char bitrate_text[BITRATE_CAPACITY];
            format_bitrate(bitrate, bitrate_text, sizeof bitrate_text);
            int width = text_width(font, bitrate_text);
            width = width < area.width ? width : area.width;
            Rect above = {area.x + area.width - width, area.y - 1 - LINE_SMALL, width, LINE_SMALL};
            label_draw(canvas, (TextStyle){font, codec_color}, (Label){above, bitrate_text});
        }
    }
}

ECHO_NP_FACES_CODE static void progress_draw(const NowPlayingFrame *frame, Canvas *canvas)
{
    Rect area = frame->area;
    Rect bar = {area.x, area.y + area.height - PROGRESS_HEIGHT, area.width, PROGRESS_HEIGHT};
    FaceBar colors = {bar, frame->theme->colors.divider, frame->theme->colors.text,
                      frame->theme->colors.selection_background};
    face_bar_draw(canvas, frame, &colors);
}

ECHO_NP_FACES_CODE void face_minimal_draw(const NowPlayingFrame *frame, Canvas *canvas)
{
    Color background = background_color(frame->theme);
    canvas_fill_rect(canvas, frame->area, background);
    cover_draw(frame, canvas);
    badge_draw(frame, canvas, background);
    title_draw(frame, canvas);
    credits_draw(frame, canvas);
    modes_draw(frame, canvas);
    queue_draw(frame, canvas);
    bottom_draw(frame, canvas);
    progress_draw(frame, canvas);
}

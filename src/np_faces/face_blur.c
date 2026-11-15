#include "core/format.h"
#include "core/i18n.h"
#include "core/resident.h"
#include "core/string_builder.h"
#include "core/utf8.h"
#include "face_text.h"
#include "faces.h"
#include "gfx/color.h"
#include "gfx/image.h"
#include "gfx/text.h"
#include "ui/label.h"

enum {
    COVER_SIDE = 112,
    COVER_TOP = 29,
    BADGE_SIDE = 40,
    BADGE_ICON_SIDE = 18,
    LINE_BIG = 20,
    LINE_SMALL = 14,
    PROGRESS_HEIGHT = 3,
    TIMES_GAP = 4,
    GLOW_COLUMNS = 8,
    GLOW_ROWS = 5,
    GLOW_SAMPLES = GLOW_COLUMNS * GLOW_ROWS,
    GLOW_CROP_NUMERATOR = 7,
    GLOW_CROP_DENOMINATOR = 10,
    GLOW_SHIFT = 8,
    GLOW_ONE = 1 << GLOW_SHIFT,
    GLOW_LIGHT = 102,
    TEXT_MARGIN = 16,
    QUEUE_CAPACITY = 48,
    DIGITS_CAPACITY = 8,
    CODEC_CAPACITY = 96
};

#define BLUR_WHITE COLOR_RGB(255, 255, 255)

typedef struct BlurRows {
    int cover_left;
    int cover_top;
    int text_x;
    int text_width;
    int title;
    int title_next;
    int artist;
    int album;
    int modes;
    int codec;
    int times;
    int times_x;
    int times_width;
} BlurRows;

typedef struct BlurWrap {
    size_t first_bytes;
    bool first_ellipsis;
    const char *second;
} BlurWrap;

ECHO_NP_FACES_DATA static const char SEPARATOR[] = " \xC2\xB7 ";
ECHO_NP_FACES_DATA static const char TIME_SEPARATOR[] = " · ";
ECHO_NP_FACES_DATA static const char TIME_SAMPLE[] = "0:00";
ECHO_NP_FACES_DATA static const char UNKNOWN_ARTIST[] = "Unknown Artist";

ECHO_NP_FACES_DATA static const BlurRows LANDSCAPE = {8,  29,  130, 182, 29, 48, 70,
                                                      89, 146, 127, 146, 8,  112};

ECHO_NP_FACES_DATA static const BlurRows PORTRAIT = {0,   29,  16,  208, 151, 170, 192,
                                                     212, 274, 254, 299, 16,  208};

ECHO_NP_FACES_CODE static BlurRows rows_of(const NowPlayingFrame *frame)
{
    if (frame->area.height <= frame->area.width) {
        return LANDSCAPE;
    }
    BlurRows rows = PORTRAIT;
    rows.cover_left = (frame->area.width - COVER_SIDE) / 2;
    rows.text_width = frame->area.width - 2 * TEXT_MARGIN;
    rows.times_width = rows.text_width;
    rows.times = frame->area.height - PROGRESS_HEIGHT - TIMES_GAP - LINE_SMALL;
    return rows;
}

ECHO_NP_FACES_CODE static Color fallback_color(const Theme *theme)
{
    return theme->colors.background == COLOR_RGB(16, 17, 20) ? COLOR_RGB(0, 0, 0)
                                                             : theme->colors.background;
}

ECHO_NP_FACES_CODE static Color *pixel_address(Canvas *canvas, int x, int y)
{
    Rect bounds = canvas->bounds;
    return &canvas->pixels[(y - bounds.y) * bounds.width + (x - bounds.x)];
}

ECHO_NP_FACES_CODE static Color lerp(Color from, Color to, int fraction)
{
    int from_red = (from >> 11) & 0x1F;
    int from_green = (from >> 5) & 0x3F;
    int from_blue = from & 0x1F;
    int to_red = (to >> 11) & 0x1F;
    int to_green = (to >> 5) & 0x3F;
    int to_blue = to & 0x1F;
    int red = (from_red * (GLOW_ONE - fraction) + to_red * fraction) >> GLOW_SHIFT;
    int green = (from_green * (GLOW_ONE - fraction) + to_green * fraction) >> GLOW_SHIFT;
    int blue = (from_blue * (GLOW_ONE - fraction) + to_blue * fraction) >> GLOW_SHIFT;
    return (Color)((red << 11) | (green << 5) | blue);
}

ECHO_NP_FACES_CODE static void glow_sample(const Image *cover, Color grid[GLOW_SAMPLES])
{
    int crop_width = cover->width * GLOW_CROP_NUMERATOR / GLOW_CROP_DENOMINATOR;
    int crop_height = cover->height * GLOW_CROP_NUMERATOR / GLOW_CROP_DENOMINATOR;
    crop_width = crop_width > 0 ? crop_width : cover->width;
    crop_height = crop_height > 0 ? crop_height : cover->height;
    int crop_left = (cover->width - crop_width) / 2;
    int crop_top = (cover->height - crop_height) / 2;
    for (int row = 0; row < GLOW_ROWS; row++) {
        int source_y = crop_top;
        if (GLOW_ROWS > 1) {
            source_y += crop_height * row / (GLOW_ROWS - 1);
        }
        source_y = source_y < cover->height ? source_y : cover->height - 1;
        for (int column = 0; column < GLOW_COLUMNS; column++) {
            int source_x = crop_left;
            if (GLOW_COLUMNS > 1) {
                source_x += crop_width * column / (GLOW_COLUMNS - 1);
            }
            source_x = source_x < cover->width ? source_x : cover->width - 1;
            Color color = cover->pixels[source_y * cover->width + source_x];
            grid[row * GLOW_COLUMNS + column] = color_scale(color, GLOW_LIGHT);
        }
    }
}

ECHO_NP_FACES_CODE static void glow_draw(const NowPlayingFrame *frame, Canvas *canvas,
                                         Color fallback)
{
    Rect area = frame->area;
    const Image *cover = &frame->cover;
    if (cover->pixels == NULL || cover->width <= 0 || cover->height <= 0) {
        canvas_fill_rect(canvas, area, fallback);
        return;
    }
    Rect visible = rect_intersection(canvas->clip, area);
    if (rect_is_empty(visible)) {
        return;
    }
    Color grid[GLOW_SAMPLES];
    glow_sample(cover, grid);
    int last_column = area.width > 1 ? area.width - 1 : 1;
    int last_row = area.height > 1 ? area.height - 1 : 1;
    uint32_t column_steps = (GLOW_COLUMNS - 1) << GLOW_SHIFT;
    uint32_t column_step = column_steps / (uint32_t)last_column;
    uint32_t column_remainder = column_steps % (uint32_t)last_column;
    uint32_t column_extra = (uint32_t)(visible.x - area.x) * column_remainder;
    uint32_t fixed_x =
        (uint32_t)(visible.x - area.x) * column_step + column_extra / (uint32_t)last_column;
    uint32_t remainder_x = column_extra % (uint32_t)last_column;
    uint32_t row_steps = (GLOW_ROWS - 1) << GLOW_SHIFT;
    for (int y = visible.y; y < visible.y + visible.height; y++) {
        uint32_t fixed_y = (uint32_t)(y - area.y) * row_steps / (uint32_t)last_row;
        int row = (int)(fixed_y >> GLOW_SHIFT);
        int fraction_y = (int)(fixed_y & (GLOW_ONE - 1));
        if (row >= GLOW_ROWS - 1) {
            row = GLOW_ROWS - 2;
            fraction_y = GLOW_ONE;
        }
        const Color *top = &grid[row * GLOW_COLUMNS];
        const Color *bottom = top + GLOW_COLUMNS;
        Color *target = pixel_address(canvas, visible.x, y);
        uint32_t fixed = fixed_x;
        uint32_t remainder = remainder_x;
        for (int x = visible.x; x < visible.x + visible.width; x++) {
            int column = (int)(fixed >> GLOW_SHIFT);
            int fraction_x = (int)(fixed & (GLOW_ONE - 1));
            if (column >= GLOW_COLUMNS - 1) {
                column = GLOW_COLUMNS - 2;
                fraction_x = GLOW_ONE;
            }
            Color upper = lerp(top[column], top[column + 1], fraction_x);
            Color lower = lerp(bottom[column], bottom[column + 1], fraction_x);
            *target++ = lerp(upper, lower, fraction_y);
            fixed += column_step;
            remainder += column_remainder;
            if (remainder >= (uint32_t)last_column) {
                remainder -= (uint32_t)last_column;
                fixed++;
            }
        }
    }
}

ECHO_NP_FACES_CODE static Rect column_area(const NowPlayingFrame *frame, const BlurRows *rows,
                                           int top, int height)
{
    int available = frame->area.width - rows->text_x;
    int width = rows->text_width < available ? rows->text_width : available;
    width = width > 0 ? width : 0;
    return (Rect){frame->area.x + rows->text_x, frame->area.y + top, width, height};
}

ECHO_NP_FACES_CODE static Rect times_area(const NowPlayingFrame *frame, const BlurRows *rows)
{
    int available = frame->area.width - rows->times_x;
    int width = rows->times_width < available ? rows->times_width : available;
    width = width > 0 ? width : 0;
    return (Rect){frame->area.x + rows->times_x, frame->area.y + rows->times, width, LINE_SMALL};
}

ECHO_NP_FACES_CODE static void cover_draw(const NowPlayingFrame *frame, Canvas *canvas,
                                          const BlurRows *rows)
{
    if (frame->cover.pixels == NULL) {
        return;
    }
    int left = frame->area.x + rows->cover_left;
    int top = frame->area.y + rows->cover_top;
    image_draw_quad(canvas, frame->cover,
                    (Quad){left, COVER_SIDE, top, COVER_SIDE, top, COVER_SIDE});
}

ECHO_NP_FACES_CODE static void badge_draw(const NowPlayingFrame *frame, Canvas *canvas,
                                          const BlurRows *rows, Color background)
{
    if (face_badge_kind(frame) == FACE_ICON_NONE) {
        return;
    }
    int centre_x = frame->area.x + rows->cover_left + COVER_SIDE / 2;
    int centre_y = frame->area.y + rows->cover_top + COVER_SIDE / 2;
    Rect badge = {centre_x - BADGE_SIDE / 2, centre_y - BADGE_SIDE / 2, BADGE_SIDE, BADGE_SIDE};
    Rect icon = {centre_x - BADGE_ICON_SIDE / 2, centre_y - BADGE_ICON_SIDE / 2, BADGE_ICON_SIDE,
                 BADGE_ICON_SIDE};
    canvas_fill_rect(canvas, badge, background);
    face_badge_draw(canvas, frame, icon, BLUR_WHITE);
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

ECHO_NP_FACES_CODE static bool has_text(const char *text)
{
    return text != NULL && text[0] != '\0';
}

ECHO_NP_FACES_CODE static bool text_starts_with(const char *text, const char *prefix)
{
    size_t index = 0;
    while (prefix[index] != '\0') {
        if (text[index] != prefix[index]) {
            return false;
        }
        index++;
    }
    return true;
}

ECHO_NP_FACES_CODE static BlurWrap wrap_title(const Font *font, const char *title, int width)
{
    BlurWrap wrap = {text_length(title), false, NULL};
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

ECHO_NP_FACES_CODE static void title_draw(const NowPlayingFrame *frame, Canvas *canvas,
                                          const BlurRows *rows)
{
    const char *title = frame->song;
    if (!has_text(title)) {
        return;
    }
    TextStyle style = {frame->theme->fonts.title, BLUR_WHITE};
    Rect first = column_area(frame, rows, rows->title, LINE_BIG);
    BlurWrap wrap = wrap_title(style.font, title, first.width);
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

ECHO_NP_FACES_CODE static void credits_draw(const NowPlayingFrame *frame, Canvas *canvas,
                                            const BlurRows *rows)
{
    const Theme *theme = frame->theme;
    if (has_text(frame->artist) && !text_equals(frame->artist, UNKNOWN_ARTIST)) {
        Rect area = column_area(frame, rows, rows->artist, LINE_BIG);
        label_draw(canvas, (TextStyle){theme->fonts.body, BLUR_WHITE},
                   (Label){area, frame->artist});
    }
    if (has_text(frame->album)) {
        Rect area = column_area(frame, rows, rows->album, LINE_BIG);
        label_draw(canvas, (TextStyle){theme->fonts.body, BLUR_WHITE}, (Label){area, frame->album});
    }
}

ECHO_NP_FACES_CODE static void append_separator(StringBuilder *builder)
{
    if (builder->length > 0) {
        string_builder_append(builder, SEPARATOR);
    }
}

ECHO_NP_FACES_CODE static void queue_text(const NowPlayingFrame *frame, StringBuilder *builder)
{
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
    i18n_format(builder, STR_MENU_POSITION_OF, arguments, 2);
}

ECHO_NP_FACES_CODE static void modes_draw(const NowPlayingFrame *frame, Canvas *canvas,
                                          const BlurRows *rows)
{
    char buffer[QUEUE_CAPACITY];
    StringBuilder builder = string_builder_make(buffer, sizeof buffer);
    if (has_text(frame->modes)) {
        const char *shuffle_songs = i18n_text(STR_MENU_SHUFFLE_SONGS);
        bool shortened = false;
        if (text_starts_with(frame->modes, shuffle_songs)) {
            const char *rest = frame->modes + text_length(shuffle_songs);
            if (*rest == '\0' || text_starts_with(rest, SEPARATOR)) {
                string_builder_append(&builder, i18n_text(STR_SETTINGS_SHUFFLE));
                string_builder_append(&builder, rest);
                shortened = true;
            }
        }
        if (!shortened) {
            string_builder_append(&builder, frame->modes);
        }
    }
    if (builder.length > 0 && frame->status->place.total > 0) {
        append_separator(&builder);
    }
    queue_text(frame, &builder);
    if (buffer[0] == '\0') {
        return;
    }
    TextStyle style = {frame->theme->fonts.small, BLUR_WHITE};
    Rect area = column_area(frame, rows, rows->modes, LINE_SMALL);
    label_draw(canvas, style,
               (Label){face_fitted_area(style.font, area, buffer, FACE_ALIGN_LEFT), buffer});
}

ECHO_NP_FACES_CODE static void codec_draw(const NowPlayingFrame *frame, Canvas *canvas,
                                          const BlurRows *rows)
{
    char buffer[CODEC_CAPACITY];
    StringBuilder builder = string_builder_make(buffer, sizeof buffer);
    if (has_text(frame->codec)) {
        string_builder_append(&builder, frame->codec);
    }
    if (has_text(frame->stream)) {
        append_separator(&builder);
        string_builder_append(&builder, frame->stream);
    }
    uint32_t bitrate = frame->status->format.bitrate_kbps;
    if (bitrate > 0) {
        char bitrate_text[CODEC_CAPACITY];
        format_bitrate(bitrate, bitrate_text, sizeof bitrate_text);
        append_separator(&builder);
        string_builder_append(&builder, bitrate_text);
    }
    if (buffer[0] == '\0') {
        return;
    }
    TextStyle style = {frame->theme->fonts.small, BLUR_WHITE};
    Rect area = column_area(frame, rows, rows->codec, LINE_SMALL);
    label_draw(canvas, style,
               (Label){face_fitted_area(style.font, area, buffer, FACE_ALIGN_LEFT), buffer});
}

ECHO_NP_FACES_CODE static void times_draw(const NowPlayingFrame *frame, Canvas *canvas,
                                          const BlurRows *rows)
{
    TextStyle style = {frame->theme->fonts.small, BLUR_WHITE};
    Rect area = times_area(frame, rows);
    char buffer[FACE_FEEDBACK_CAPACITY];
    const char *feedback =
        frame->volume_shown ? face_feedback_text(frame, buffer, sizeof buffer) : NULL;
    if (feedback != NULL) {
        label_draw(
            canvas, style,
            (Label){face_fitted_area(style.font, area, feedback, FACE_ALIGN_LEFT), feedback});
        return;
    }
    char elapsed[FACE_TIME_CAPACITY];
    char total[FACE_TIME_CAPACITY];
    format_duration(frame->shown_ms, elapsed, sizeof elapsed);
    format_duration(frame->status->duration_ms, total, sizeof total);
    int elapsed_width = text_width(style.font, TIME_SAMPLE);
    elapsed_width = elapsed_width < area.width ? elapsed_width : area.width;
    int separator_width = text_width(style.font, TIME_SEPARATOR);
    separator_width =
        separator_width < area.width - elapsed_width ? separator_width : area.width - elapsed_width;
    int total_width = area.width - elapsed_width - separator_width;
    Rect separator = {area.x + elapsed_width, area.y, separator_width, LINE_SMALL};
    Rect right = {separator.x + separator_width, area.y, total_width, LINE_SMALL};
    label_draw(canvas, style, (Label){(Rect){area.x, area.y, elapsed_width, LINE_SMALL}, elapsed});
    label_draw(canvas, style, (Label){separator, TIME_SEPARATOR});
    label_draw(canvas, style, (Label){right, total});
}

ECHO_NP_FACES_CODE static void progress_draw(const NowPlayingFrame *frame, Canvas *canvas)
{
    Rect area = frame->area;
    Rect bar = {area.x, area.y + area.height - PROGRESS_HEIGHT, area.width, PROGRESS_HEIGHT};
    FaceBar colors = {bar, frame->theme->colors.divider, frame->theme->colors.text,
                      frame->theme->colors.selection_background};
    face_bar_draw(canvas, frame, &colors);
}

ECHO_NP_FACES_CODE void face_blur_draw(const NowPlayingFrame *frame, Canvas *canvas)
{
    BlurRows rows = rows_of(frame);
    Color background = fallback_color(frame->theme);
    glow_draw(frame, canvas, background);
    cover_draw(frame, canvas, &rows);
    badge_draw(frame, canvas, &rows, background);
    title_draw(frame, canvas, &rows);
    credits_draw(frame, canvas, &rows);
    modes_draw(frame, canvas, &rows);
    codec_draw(frame, canvas, &rows);
    times_draw(frame, canvas, &rows);
    progress_draw(frame, canvas);
}

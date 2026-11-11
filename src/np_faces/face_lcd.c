#include "core/resident.h"
#include "face_text.h"
#include "faces.h"
#include "np_faces/face_shapes.h"
#include "ui/label.h"

enum {
    INK = COLOR_RGB(24, 36, 16),
    GLASS = COLOR_RGB(176, 196, 104),
    GHOST = COLOR_RGB(144, 168, 80),
    SHADOW = COLOR_RGB(136, 156, 72),
    BEZEL = COLOR_RGB(32, 32, 40),
    LEGEND_COUNT = 5,
    LEGEND_CAPACITY = 8,
    LEGEND_HEIGHT = 15,
    GLASS_INSET = 6,
    LIP_HEIGHT = 2,
    TOP_INSET = 3,
    ICON_ROW = 7,
    ICON_WIDTH = 8,
    ICON_HEIGHT = 9,
    BADGE_SIDE = 18,
    BIG_COLON_LEFT = 22,
    BIG_COLON_RIGHT = 78,
    MID_PITCH = 16,
    CELL_COUNT = 8,
    CELL_WIDTH = 7,
    CELL_HEIGHT = 3,
    CELL_STEP = 5,
    BAND_PITCH = 9,
    TEXT_HEIGHT = 20,
    LATTICE_PITCH = 3,
    PROGRESS_PITCH = 4,
    PROGRESS_CELL_WIDTH = 3,
    PROGRESS_CELL_HEIGHT = 4,
    INFO_GAP = 8,
    DIGIT_COUNT = 5,
    MAX_TRACK = 999,
    MAX_LEVEL = 99,
    MAX_TIME_SECONDS = 35999,
    SECONDS_PER_HOUR = 3600,
    SECONDS_PER_MINUTE = 60,
    MILLISECONDS_PER_SECOND = 1000,
    BEAT_PERIOD = 6,
    BEAT_STRONG = 3,
    BEAT_WEAK = 1,
    SPECTRUM_MAX = 8,
    LANDSCAPE_MIN_WIDTH = 200
};

typedef struct LcdLegend {
    char text[LEGEND_CAPACITY];
    uint8_t x;
    uint8_t y;
} LcdLegend;

typedef struct LcdLayout {
    int16_t glass_x;
    int16_t glass_width;
    uint8_t play_x;
    uint8_t pause_x;
    uint8_t bands;
    uint8_t track_count;
    FaceAlign align;
    LcdLegend legends[LEGEND_COUNT];
    Point codec;
    Point track_label;
    Point track;
    Point big;
    Point spectrum;
    Point badge;
    Rect title;
    Rect artist;
    Rect album;
    Point progress;
    Rect info;
} LcdLayout;

typedef struct LcdPane {
    Canvas *canvas;
    const NowPlayingFrame *frame;
    const LcdLayout *layout;
    int top;
} LcdPane;

typedef struct LcdReadout {
    uint8_t masks[DIGIT_COUNT];
    bool hours;
    bool colon;
} LcdReadout;

ECHO_NP_FACES_DATA static const uint32_t HASH_MIX_FRAME = 0x9E3779B1u;
ECHO_NP_FACES_DATA static const uint32_t HASH_MIX_BAND = 0x85EBCA6Bu;
ECHO_NP_FACES_DATA static const uint32_t HASH_MULTIPLY = 0x2C1B3C6Du;

ECHO_NP_FACES_DATA static const char TRACK_LEGEND[] = "TRACK";

ECHO_NP_FACES_DATA static const uint8_t BIG_PITCH[DIGIT_COUNT] = {0, 30, 56, 86, 112};

ECHO_NP_FACES_DATA static const LcdLayout LANDSCAPE = {
    .glass_x = 5,
    .glass_width = 310,
    .play_x = 12,
    .pause_x = 25,
    .bands = 10,
    .track_count = 74,
    .align = FACE_ALIGN_LEFT,
    .legends =
        {{"SHUFFLE", 41, 4}, {"ALBUM", 101, 4}, {"REPEAT", 156, 4}, {"1", 208, 4}, {"ALL", 219, 4}},
    .codec = {308, 4},
    .track_label = {12, 23},
    .track = {12, 39},
    .big = {68, 23},
    .spectrum = {220, 60},
    .badge = {255, 35},
    .title = {12, 68, 296, TEXT_HEIGHT},
    .artist = {12, 90, 296, TEXT_HEIGHT},
    .album = {0, 0, 0, 0},
    .progress = {12, 115},
    .info = {12, 122, 296, LEGEND_HEIGHT},
};

ECHO_NP_FACES_DATA static const LcdLayout PORTRAIT = {
    .glass_x = 4,
    .glass_width = 162,
    .play_x = 10,
    .pause_x = 23,
    .bands = 16,
    .track_count = 37,
    .align = FACE_ALIGN_CENTRE,
    .legends =
        {{"SHUFFLE", 40, 4}, {"ALBUM", 100, 4}, {"REPEAT", 10, 21}, {"1", 62, 21}, {"ALL", 73, 21}},
    .codec = {160, 21},
    .track_label = {37, 55},
    .track = {85, 44},
    .big = {16, 76},
    .spectrum = {14, 160},
    .badge = {76, 134},
    .title = {10, 171, 150, TEXT_HEIGHT},
    .artist = {10, 194, 150, TEXT_HEIGHT},
    .album = {10, 214, 150, TEXT_HEIGHT},
    .progress = {11, 241},
    .info = {10, 248, 150, LEGEND_HEIGHT},
};

ECHO_NP_FACES_CODE static Color lit_colour(bool lit)
{
    return lit ? INK : GHOST;
}

ECHO_NP_FACES_CODE static bool has_text(const char *text)
{
    return text != NULL && text[0] != '\0';
}

ECHO_NP_FACES_CODE static Rect lowered(const LcdPane *pane, Rect area)
{
    return (Rect){area.x, pane->top + area.y, area.width, area.height};
}

ECHO_NP_FACES_CODE static Point placed(const LcdPane *pane, Point point)
{
    return (Point){point.x, pane->top + point.y};
}

ECHO_NP_FACES_CODE static void cell_fill(const LcdPane *pane, Rect cell, Color color)
{
    canvas_fill_rect(pane->canvas, cell, color);
}

ECHO_NP_FACES_CODE static void glass_draw(const LcdPane *pane)
{
    const LcdLayout *layout = pane->layout;
    Rect glass = {layout->glass_x, pane->top, layout->glass_width,
                  pane->frame->area.height - GLASS_INSET};
    cell_fill(pane, pane->frame->area, BEZEL);
    cell_fill(pane, glass, GLASS);
    cell_fill(pane, (Rect){glass.x, glass.y, glass.width, LIP_HEIGHT}, SHADOW);
    for (int corner = 0; corner < 4; corner++) {
        Rect pixel = {glass.x + (corner & 1) * (glass.width - 1),
                      glass.y + (corner >> 1) * (glass.height - 1), 1, 1};
        cell_fill(pane, pixel, BEZEL);
    }
}

ECHO_NP_FACES_CODE static bool legend_lit(const PlayerStatus *status, int index)
{
    switch (index) {
    case 0:
        return status->shuffle != SHUFFLE_OFF;
    case 1:
        return status->shuffle == SHUFFLE_ALBUMS;
    case 2:
        return status->repeat != REPEAT_OFF;
    case 3:
        return status->repeat == REPEAT_ONE;
    default:
        return status->repeat == REPEAT_ALL;
    }
}

ECHO_NP_FACES_CODE static void legends_draw(const LcdPane *pane)
{
    const Font *font = pane->frame->theme->fonts.small;
    for (int index = 0; index < LEGEND_COUNT; index++) {
        const LcdLegend *legend = &pane->layout->legends[index];
        Color color = lit_colour(legend_lit(pane->frame->status, index));
        Rect area = lowered(
            pane, (Rect){legend->x, legend->y, text_width(font, legend->text), LEGEND_HEIGHT});
        label_draw(pane->canvas, (TextStyle){font, color}, (Label){area, legend->text});
    }
}

ECHO_NP_FACES_CODE static void transport_draw(const LcdPane *pane)
{
    const LcdLayout *layout = pane->layout;
    bool paused = pane->frame->status->state == PLAYER_PAUSED;
    Rect play = lowered(pane, (Rect){layout->play_x, ICON_ROW, ICON_WIDTH, ICON_HEIGHT});
    Rect pause = lowered(pane, (Rect){layout->pause_x, ICON_ROW, ICON_WIDTH, ICON_HEIGHT});
    face_icon_draw(pane->canvas, FACE_ICON_PLAY, play, lit_colour(pane->frame->playing));
    face_icon_draw(pane->canvas, FACE_ICON_PAUSE, pause, lit_colour(paused));
}

ECHO_NP_FACES_CODE static void codec_draw(const LcdPane *pane)
{
    const Font *font = pane->frame->theme->fonts.small;
    int width = face_codec_width(font, pane->frame->codec);
    Point right = placed(pane, pane->layout->codec);
    FaceLine line = {
        font, {right.x - width, right.y, width, LEGEND_HEIGHT}, INK, INK, FACE_ALIGN_LEFT};
    face_codec_draw(pane->canvas, &line, pane->frame->codec);
}

ECHO_NP_FACES_CODE static uint8_t digit_at(unsigned value, unsigned divisor, bool always)
{
    return value >= divisor || always ? shape_digit_mask(value / divisor % 10) : 0;
}

ECHO_NP_FACES_CODE static LcdReadout readout_of(const NowPlayingFrame *frame)
{
    unsigned seconds = frame->shown_ms / MILLISECONDS_PER_SECOND;
    unsigned level = frame->status->volume < MAX_LEVEL ? frame->status->volume : MAX_LEVEL;
    if (frame->volume_shown) {
        return (LcdReadout){
            .masks = {0, 0, 0, digit_at(level, 10, false), digit_at(level, 1, true)}};
    }
    seconds = seconds < MAX_TIME_SECONDS ? seconds : MAX_TIME_SECONDS;
    unsigned hours = seconds / SECONDS_PER_HOUR;
    unsigned minutes = seconds / SECONDS_PER_MINUTE % SECONDS_PER_MINUTE;
    unsigned rest = seconds % SECONDS_PER_MINUTE;
    return (LcdReadout){{digit_at(hours, 1, false), digit_at(minutes, 10, hours > 0),
                         digit_at(minutes, 1, true), digit_at(rest, 10, true),
                         digit_at(rest, 1, true)},
                        hours > 0,
                        true};
}

ECHO_NP_FACES_CODE static void big_draw(const LcdPane *pane)
{
    SegmentStyle style = {22, 40, 5, 8, INK, GHOST};
    LcdReadout readout = readout_of(pane->frame);
    Point origin = placed(pane, pane->layout->big);
    for (int index = 0; index < DIGIT_COUNT; index++) {
        shape_digit(pane->canvas, &style, (Point){origin.x + BIG_PITCH[index], origin.y},
                    readout.masks[index]);
    }
    shape_colon(pane->canvas, &style, (Point){origin.x + BIG_COLON_LEFT, origin.y}, readout.hours);
    shape_colon(pane->canvas, &style, (Point){origin.x + BIG_COLON_RIGHT, origin.y}, readout.colon);
}

ECHO_NP_FACES_CODE static void track_draw(const LcdPane *pane)
{
    const Font *font = pane->frame->theme->fonts.small;
    SegmentStyle style = {13, 24, 3, 8, INK, GHOST};
    unsigned number = pane->frame->status->place.number;
    Point label = placed(pane, pane->layout->track_label);
    Point origin = placed(pane, pane->layout->track);
    number = number < MAX_TRACK ? number : MAX_TRACK;
    label_draw(
        pane->canvas, (TextStyle){font, INK},
        (Label){{label.x, label.y, text_width(font, TRACK_LEGEND), LEGEND_HEIGHT}, TRACK_LEGEND});
    shape_digit(pane->canvas, &style, origin, digit_at(number, 100, false));
    shape_digit(pane->canvas, &style, (Point){origin.x + MID_PITCH, origin.y},
                digit_at(number, 10, false));
    shape_digit(pane->canvas, &style, (Point){origin.x + 2 * MID_PITCH, origin.y},
                digit_at(number, 1, true));
}

ECHO_NP_FACES_CODE static uint32_t spectrum_hash(uint32_t frame, uint32_t band)
{
    uint32_t value = frame * HASH_MIX_FRAME + band * HASH_MIX_BAND;
    value ^= value >> 15;
    value *= HASH_MULTIPLY;
    return value ^ (value >> 12);
}

ECHO_NP_FACES_CODE static int spectrum_raw(uint32_t frame, int band, int bands)
{
    uint32_t phase = frame % BEAT_PERIOD;
    int beat = phase == 0 ? BEAT_STRONG : phase == 1 ? BEAT_WEAK : 0;
    uint32_t value = spectrum_hash(frame, (uint32_t)band);
    return 5 - 4 * band / bands + (band < bands / 2 ? beat : 0) + (int)(value >> 30) -
           (int)((value >> 28) & 3) / 2;
}

ECHO_NP_FACES_CODE static int spectrum_level(uint32_t frame, int band, int bands)
{
    int level = spectrum_raw(frame, band, bands);
    int echo = spectrum_raw(frame - 1, band, bands) - 2;
    int tail = spectrum_raw(frame - 2, band, bands) - 4;
    level = level > echo ? level : echo;
    level = level > tail ? level : tail;
    return level < 0 ? 0 : level > SPECTRUM_MAX ? SPECTRUM_MAX : level;
}

ECHO_NP_FACES_CODE static void band_draw(const LcdPane *pane, int band, int level)
{
    Point base = placed(pane, pane->layout->spectrum);
    for (int cell = 0; cell < CELL_COUNT; cell++) {
        Rect area = {base.x + BAND_PITCH * band, base.y - CELL_STEP * cell, CELL_WIDTH,
                     CELL_HEIGHT};
        cell_fill(pane, area, lit_colour(cell < level));
    }
}

ECHO_NP_FACES_CODE static void spectrum_draw(const LcdPane *pane)
{
    const NowPlayingFrame *frame = pane->frame;
    Point badge = placed(pane, pane->layout->badge);
    if (face_badge_kind(frame) != FACE_ICON_NONE) {
        face_badge_draw(pane->canvas, frame, (Rect){badge.x, badge.y, BADGE_SIDE, BADGE_SIDE}, INK);
        return;
    }
    int bands = pane->layout->bands;
    for (int band = 0; band < bands; band++) {
        band_draw(pane, band, frame->playing ? spectrum_level(frame->tick, band, bands) : 0);
    }
}

ECHO_NP_FACES_CODE static FaceLine ink_line(const LcdPane *pane, const Font *font, Rect area)
{
    return (FaceLine){font, lowered(pane, area), INK, INK, pane->layout->align};
}

ECHO_NP_FACES_CODE static const char *lyric_or(const NowPlayingFrame *frame, const char *text)
{
    return has_text(frame->lyric) ? frame->lyric : text;
}

ECHO_NP_FACES_CODE static void title_draw(const LcdPane *pane)
{
    FaceLine line = ink_line(pane, pane->frame->theme->fonts.title, pane->layout->title);
    cell_fill(pane, line.area, GHOST);
    shape_lattice(pane->canvas, line.area, LATTICE_PITCH, GLASS);
    face_line(pane->canvas, pane->frame, &line, pane->frame->song);
}

ECHO_NP_FACES_CODE static void credits_draw(const LcdPane *pane)
{
    const NowPlayingFrame *frame = pane->frame;
    const Font *font = frame->theme->fonts.body;
    FaceLine artist = ink_line(pane, font, pane->layout->artist);
    if (pane->layout->album.width == 0) {
        face_line(pane->canvas, frame, &artist, lyric_or(frame, frame->artist));
        return;
    }
    FaceLine album = ink_line(pane, font, pane->layout->album);
    face_line(pane->canvas, frame, &artist, frame->artist);
    face_line(pane->canvas, frame, &album, lyric_or(frame, frame->album));
}

ECHO_NP_FACES_CODE static void progress_draw(const LcdPane *pane)
{
    Point origin = placed(pane, pane->layout->progress);
    int filled = face_bar_fill(pane->frame, pane->layout->track_count).width;
    for (int index = 0; index < pane->layout->track_count; index++) {
        Rect area = {origin.x + PROGRESS_PITCH * index, origin.y, PROGRESS_CELL_WIDTH,
                     PROGRESS_CELL_HEIGHT};
        cell_fill(pane, area, lit_colour(index < filled));
    }
}

ECHO_NP_FACES_CODE static void stream_draw(const LcdPane *pane, const FaceTimes *times)
{
    const Font *font = pane->frame->theme->fonts.small;
    FaceLine line = ink_line(pane, font, pane->layout->info);
    int width = text_width(font, times->remaining);
    Rect right = {line.area.x + line.area.width - width, line.area.y, width, line.area.height};
    label_draw(pane->canvas, (TextStyle){font, INK}, (Label){right, times->remaining});
    if (has_text(pane->frame->stream)) {
        line.area.width -= width + INFO_GAP;
        label_draw(pane->canvas, (TextStyle){font, INK}, (Label){line.area, pane->frame->stream});
    }
}

ECHO_NP_FACES_CODE static void info_draw(const LcdPane *pane)
{
    char buffer[FACE_FEEDBACK_CAPACITY];
    const char *feedback = face_feedback_text(pane->frame, buffer, sizeof buffer);
    FaceTimes times;
    if (feedback != NULL) {
        FaceLine line = ink_line(pane, pane->frame->theme->fonts.small, pane->layout->info);
        Rect fitted = face_fitted_area(line.font, line.area, feedback, line.align);
        label_draw(pane->canvas, (TextStyle){line.font, INK}, (Label){fitted, feedback});
        return;
    }
    face_times_text(pane->frame, &times);
    stream_draw(pane, &times);
}

ECHO_NP_FACES_CODE void face_lcd_draw(const NowPlayingFrame *frame, Canvas *canvas)
{
    bool wide = frame->area.width >= LANDSCAPE_MIN_WIDTH;
    LcdPane pane = {canvas, frame, wide ? &LANDSCAPE : &PORTRAIT, frame->area.y + TOP_INSET};
    glass_draw(&pane);
    transport_draw(&pane);
    legends_draw(&pane);
    codec_draw(&pane);
    track_draw(&pane);
    big_draw(&pane);
    spectrum_draw(&pane);
    title_draw(&pane);
    credits_draw(&pane);
    progress_draw(&pane);
    info_draw(&pane);
}

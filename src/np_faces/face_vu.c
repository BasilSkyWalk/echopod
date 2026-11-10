#include "core/resident.h"
#include "face_text.h"
#include "faces.h"
#include "np_faces/face_shapes.h"
#include "ui/label.h"

#define VU_PLATE COLOR_RGB(32, 28, 24)
#define VU_BEZEL COLOR_RGB(8, 8, 8)
#define VU_FACE_TOP COLOR_RGB(248, 236, 192)
#define VU_FACE_BOTTOM COLOR_RGB(240, 200, 120)
#define VU_INK COLOR_RGB(40, 28, 16)
#define VU_RED COLOR_RGB(160, 24, 16)
#define VU_CREAM COLOR_RGB(240, 232, 216)
#define VU_MUTED COLOR_RGB(168, 152, 128)
#define VU_AMBER COLOR_RGB(248, 168, 48)
#define VU_TRACK COLOR_RGB(72, 64, 56)

enum {
    DIAL_WIDTH = 146,
    DIAL_HEIGHT = 72,
    FACE_INSET = 3,
    FACE_WIDTH = 140,
    FACE_HEIGHT = 54,
    PIVOT_X = 73,
    PIVOT_Y = 112,
    ARC_INNER = 86,
    ARC_OUTER = 88,
    ARC_RED_OUTER = 91,
    TICK_WIDTH = 20,
    TICK_MAJOR = 93,
    TICK_MINOR = 91,
    LABEL_RADIUS = 100,
    LABEL_HALF_HEIGHT = 7,
    LABEL_HEIGHT = 15,
    NEEDLE_LENGTH = 95,
    NEEDLE_WIDTH = 24,
    NEEDLE_MAX = 62,
    ANGLE_FIRST = -23,
    ANGLE_ZERO = 12,
    ANGLE_LAST = 25,
    PEAK = 46,
    PEAK_RADIUS = 48,
    SWAY_BASE = 7680,
    SWAY_SCALE = 256,
    SWAY_TERMS = 3,
    PEAK_PERIOD = 53,
    PEAK_WIDTH = 2,
    PEAK_KICK = 10,
    CHANNEL_SKEW = 29,
    MARK_COUNT = 11,
    BADGE_SIZE = 40,
    BADGE_ICON = 18,
    BADGE_ICON_INSET = 11,
    ROW_TITLE = 0,
    ROW_SECOND = 1,
    ROW_ARTIST = 1,
    ROW_ALBUM = 2,
    ROW_FORMAT = 3,
    ROW_MODES = 4,
    ROW_TIMES = 5,
    ROW_COUNT = 6,
    FONT_TITLE = 0,
    FONT_BODY = 1,
    FONT_SMALL = 2,
    HALF = 2
};

typedef struct VuMark {
    int8_t angle;
    uint8_t reach;
    char text[3];
} VuMark;

typedef struct VuText {
    Rect area;
    Color emphasis;
    Color color;
    uint8_t font;
    uint8_t align;
} VuText;

typedef struct VuLayout {
    VuText text[ROW_COUNT];
    Rect bar;
    Rect icon;
} VuLayout;

typedef struct VuMono {
    Point origin;
    const char *letter;
    int value;
} VuMono;

ECHO_NP_FACES_DATA static const VuMark MARKS[MARK_COUNT] = {
    {-23, TICK_MAJOR, "20"}, {-13, TICK_MAJOR, "10"}, {-9, TICK_MINOR, ""},  {-6, TICK_MAJOR, "5"},
    {0, TICK_MAJOR, "3"},    {3, TICK_MINOR, ""},     {7, TICK_MINOR, ""},   {12, TICK_MAJOR, "0"},
    {17, TICK_MINOR, ""},    {21, TICK_MINOR, ""},    {25, TICK_MAJOR, "+3"}};

ECHO_NP_FACES_DATA static const VuLayout LAND = {
    .text = {[ROW_TITLE] = {{10, 102, 300, 20}, VU_CREAM, VU_CREAM, FONT_TITLE, FACE_ALIGN_CENTRE},
             [ROW_SECOND] = {{10, 121, 300, 20}, VU_CREAM, VU_MUTED, FONT_BODY, FACE_ALIGN_CENTRE},
             [ROW_TIMES] = {{10, 147, 300, 16}, VU_CREAM, VU_MUTED, FONT_SMALL, FACE_ALIGN_LEFT}},
    .bar = {84, 154, 170, 3},
    .icon = {66, 150, 10, 10},
};

ECHO_NP_FACES_DATA static const VuLayout PORT = {
    .text = {[ROW_TITLE] = {{10, 174, 150, 22}, VU_CREAM, VU_CREAM, FONT_TITLE, FACE_ALIGN_CENTRE},
             [ROW_ARTIST] = {{10, 196, 150, 22}, VU_MUTED, VU_MUTED, FONT_BODY, FACE_ALIGN_CENTRE},
             [ROW_ALBUM] = {{10, 218, 150, 22}, VU_CREAM, VU_MUTED, FONT_BODY, FACE_ALIGN_CENTRE},
             [ROW_FORMAT] = {{10, 244, 150, 16}, VU_CREAM, VU_MUTED, FONT_SMALL, FACE_ALIGN_CENTRE},
             [ROW_MODES] = {{10, 278, 150, 16}, VU_MUTED, VU_MUTED, FONT_SMALL, FACE_ALIGN_CENTRE},
             [ROW_TIMES] = {{10, 302, 150, 16}, VU_CREAM, VU_MUTED, FONT_SMALL, FACE_ALIGN_LEFT}},
    .bar = {10, 297, 150, 3},
    .icon = {80, 305, 10, 10},
};

ECHO_NP_FACES_DATA static const uint8_t SWAY_WEIGHT[SWAY_TERMS] = {11, 7, 3};
ECHO_NP_FACES_DATA static const uint8_t SWAY_RATE[2][SWAY_TERMS] = {{3, 7, 17}, {4, 6, 13}};
ECHO_NP_FACES_DATA static const uint8_t SWAY_PHASE[2][SWAY_TERMS] = {{0, 64, 0}, {96, 160, 48}};

ECHO_NP_FACES_DATA static const char LEGEND[] = "VU";
ECHO_NP_FACES_DATA static const char LETTER_LEFT[] = "L";
ECHO_NP_FACES_DATA static const char LETTER_RIGHT[] = "R";

ECHO_NP_FACES_CODE static int vu_needle(uint32_t tick, uint32_t channel, bool playing)
{
    if (!playing) {
        return 0;
    }
    int sum = SWAY_BASE;
    for (int term = 0; term < SWAY_TERMS; term++) {
        uint8_t phase = (uint8_t)(tick * SWAY_RATE[channel][term] + SWAY_PHASE[channel][term]);
        sum += SWAY_WEIGHT[term] * shape_sine(phase);
    }
    int value = sum / SWAY_SCALE +
                ((tick + CHANNEL_SKEW * channel) % PEAK_PERIOD < PEAK_WIDTH ? PEAK_KICK : 0);
    return value > NEEDLE_MAX ? NEEDLE_MAX : value;
}

ECHO_NP_FACES_CODE static FaceLine text_line(const NowPlayingFrame *frame, const VuText *text)
{
    const ThemeFonts *fonts = &frame->theme->fonts;
    const Font *font = text->font == FONT_TITLE  ? fonts->title
                       : text->font == FONT_BODY ? fonts->body
                                                 : fonts->small;
    return (FaceLine){font, text->area, text->emphasis, text->color, (FaceAlign)text->align};
}

ECHO_NP_FACES_CODE static FaceLine plain_line(const Font *font, Rect area, Color color,
                                              FaceAlign align)
{
    return (FaceLine){font, area, color, color, align};
}

ECHO_NP_FACES_CODE static Point pivot_of(Point origin)
{
    return (Point){origin.x + PIVOT_X, origin.y + PIVOT_Y};
}

ECHO_NP_FACES_CODE static int16_t centre_q4(int pixel)
{
    return (int16_t)(pixel * SHAPE_Q4 + SHAPE_Q4 / HALF);
}

ECHO_NP_FACES_CODE static Canvas dial_face(Canvas *canvas, Point origin)
{
    shape_rounded(canvas, (Rect){origin.x, origin.y, DIAL_WIDTH, DIAL_HEIGHT}, 4, VU_BEZEL);
    Rect face = {origin.x + FACE_INSET, origin.y + FACE_INSET, FACE_WIDTH, FACE_HEIGHT};
    canvas_fill_gradient(canvas, face, VU_FACE_TOP, VU_FACE_BOTTOM);
    return canvas_clipped(canvas, face);
}

ECHO_NP_FACES_CODE static int arc_x(Point pivot, int angle, int radius)
{
    return shape_polar(pivot, (uint8_t)angle, radius).x;
}

ECHO_NP_FACES_CODE static void dial_arcs(Canvas *face, Point origin)
{
    Point pivot = pivot_of(origin);
    int left = arc_x(pivot, ANGLE_FIRST, ARC_OUTER) - 1;
    int split = arc_x(pivot, ANGLE_ZERO, ARC_OUTER);
    int right = arc_x(pivot, ANGLE_LAST, ARC_RED_OUTER) + 2;
    ShapeRing ring = {centre_q4(pivot.x), centre_q4(pivot.y), ARC_OUTER * SHAPE_Q4,
                      ARC_INNER * SHAPE_Q4};
    Canvas ink = canvas_clipped(face, (Rect){left, origin.y, split - left, DIAL_HEIGHT});
    shape_ring(&ink, ring, VU_INK);
    ring.outer_q4 = ARC_RED_OUTER * SHAPE_Q4;
    Canvas red = canvas_clipped(face, (Rect){split, origin.y, right - split, DIAL_HEIGHT});
    shape_ring(&red, ring, VU_RED);
}

ECHO_NP_FACES_CODE static void dial_mark(Canvas *face, const NowPlayingFrame *frame,
                                         const VuMark *mark, Point pivot)
{
    Color color = mark->angle > ANGLE_ZERO ? VU_RED : VU_INK;
    ShapeSpoke spoke = {pivot, (uint8_t)mark->angle, TICK_WIDTH, ARC_OUTER, mark->reach};
    shape_spoke(face, &spoke, color);
    if (mark->text[0] == '\0') {
        return;
    }
    const Font *font = frame->theme->fonts.small;
    Point centre = shape_polar(pivot, (uint8_t)mark->angle, LABEL_RADIUS);
    int width = text_width(font, mark->text);
    Rect box = {centre.x - width / HALF, centre.y - LABEL_HALF_HEIGHT, width, LABEL_HEIGHT};
    label_draw(face, (TextStyle){font, color}, (Label){box, mark->text});
}

ECHO_NP_FACES_CODE static void dial_scale(Canvas *face, const NowPlayingFrame *frame, Point origin)
{
    dial_arcs(face, origin);
    for (int index = 0; index < MARK_COUNT; index++) {
        dial_mark(face, frame, &MARKS[index], pivot_of(origin));
    }
    Rect legend = {origin.x + FACE_INSET, origin.y + 38, FACE_WIDTH, 18};
    FaceLine line = plain_line(frame->theme->fonts.title, legend, VU_INK, FACE_ALIGN_CENTRE);
    face_line(face, frame, &line, LEGEND);
}

ECHO_NP_FACES_CODE static void dial_needle(Canvas *face, Point origin, int value, Color color)
{
    uint8_t angle = (uint8_t)(value * 7 / 8 - 28);
    ShapeSpoke spoke = {pivot_of(origin), angle, NEEDLE_WIDTH, 0, NEEDLE_LENGTH};
    shape_spoke(face, &spoke, color);
}

ECHO_NP_FACES_CODE static void lamp(Canvas *canvas, Point centre, bool lit)
{
    ShapeRing ring = {centre_q4(centre.x), centre_q4(centre.y), PEAK_RADIUS, 0};
    shape_ring(canvas, ring, lit ? VU_AMBER : VU_TRACK);
}

ECHO_NP_FACES_CODE static void mono_dial(Canvas *canvas, const NowPlayingFrame *frame,
                                         const VuMono *mono)
{
    Point origin = mono->origin;
    Canvas face = dial_face(canvas, origin);
    dial_scale(&face, frame, origin);
    Rect letter = {origin.x + 8, origin.y + 57, 12, 12};
    FaceLine line = plain_line(frame->theme->fonts.small, letter, VU_MUTED, FACE_ALIGN_LEFT);
    face_line(canvas, frame, &line, mono->letter);
    lamp(&face, (Point){origin.x + 131, origin.y + 49}, mono->value >= PEAK);
    dial_needle(&face, origin, mono->value, VU_INK);
}

ECHO_NP_FACES_CODE static void stereo_letters(Canvas *face, const NowPlayingFrame *frame,
                                              Point origin)
{
    const Font *font = frame->theme->fonts.small;
    Rect left_area = {origin.x + 10, origin.y + 40, 10, 16};
    FaceLine left = plain_line(font, left_area, VU_INK, FACE_ALIGN_LEFT);
    face_line(face, frame, &left, LETTER_LEFT);
    Rect right_area = {origin.x + 126, origin.y + 40, 10, 16};
    FaceLine right = plain_line(font, right_area, VU_RED, FACE_ALIGN_RIGHT);
    face_line(face, frame, &right, LETTER_RIGHT);
}

ECHO_NP_FACES_CODE static void stereo_dial(Canvas *canvas, const NowPlayingFrame *frame,
                                           Point origin)
{
    int left = vu_needle(frame->tick, 0, frame->playing);
    int right = vu_needle(frame->tick, 1, frame->playing);
    Canvas face = dial_face(canvas, origin);
    dial_scale(&face, frame, origin);
    stereo_letters(&face, frame, origin);
    lamp(canvas, (Point){origin.x + PIVOT_X, origin.y + 63}, left >= PEAK || right >= PEAK);
    dial_needle(&face, origin, left, VU_INK);
    dial_needle(&face, origin, right, VU_RED);
}

ECHO_NP_FACES_CODE static void badge(Canvas *canvas, const NowPlayingFrame *frame, Point at)
{
    if (face_badge_kind(frame) == FACE_ICON_NONE) {
        return;
    }
    Rect box = {at.x, at.y, BADGE_SIZE, BADGE_SIZE};
    canvas_fill_rect(canvas, box, VU_BEZEL);
    face_outline(canvas, box, VU_TRACK);
    Rect icon = {at.x + BADGE_ICON_INSET, at.y + BADGE_ICON_INSET, BADGE_ICON, BADGE_ICON};
    face_badge_draw(canvas, frame, icon, VU_CREAM);
}

ECHO_NP_FACES_CODE static void transport(Canvas *canvas, const NowPlayingFrame *frame,
                                         const VuLayout *layout)
{
    FaceBar bar = {layout->bar, VU_TRACK, VU_AMBER, VU_CREAM};
    face_bar_draw(canvas, frame, &bar);
    face_icon_draw(canvas, frame->playing ? FACE_ICON_PLAY : FACE_ICON_PAUSE, layout->icon,
                   frame->playing ? VU_AMBER : VU_MUTED);
    FaceLine times = text_line(frame, &layout->text[ROW_TIMES]);
    face_times_draw(canvas, frame, &times);
}

ECHO_NP_FACES_CODE static void still_text(Canvas *canvas, const FaceLine *line, const char *text,
                                          Color color)
{
    Rect fitted = face_fitted_area(line->font, line->area, text, line->align);
    label_draw(canvas, (TextStyle){line->font, color}, (Label){fitted, text});
}

ECHO_NP_FACES_CODE static void landscape(Canvas *canvas, const NowPlayingFrame *frame)
{
    VuMono left = {{10, 28}, LETTER_LEFT, vu_needle(frame->tick, 0, frame->playing)};
    VuMono right = {{164, 28}, LETTER_RIGHT, vu_needle(frame->tick, 1, frame->playing)};
    mono_dial(canvas, frame, &left);
    mono_dial(canvas, frame, &right);
    badge(canvas, frame, (Point){140, 44});
    FaceLine title = text_line(frame, &LAND.text[ROW_TITLE]);
    face_line(canvas, frame, &title, frame->song);
    FaceLine second = text_line(frame, &LAND.text[ROW_SECOND]);
    face_feedback_line(canvas, frame, &second, frame->artist);
    transport(canvas, frame, &LAND);
}

ECHO_NP_FACES_CODE static void portrait_format(Canvas *canvas, const NowPlayingFrame *frame)
{
    FaceLine line = text_line(frame, &PORT.text[ROW_FORMAT]);
    char buffer[FACE_FEEDBACK_CAPACITY];
    const char *feedback = face_feedback_text(frame, buffer, sizeof buffer);
    if (feedback == NULL) {
        face_format_row(canvas, frame, &line);
    } else {
        still_text(canvas, &line, feedback, line.emphasis);
    }
}

ECHO_NP_FACES_CODE static void portrait_text(Canvas *canvas, const NowPlayingFrame *frame)
{
    FaceLine title = text_line(frame, &PORT.text[ROW_TITLE]);
    face_line(canvas, frame, &title, frame->song);
    FaceLine artist = text_line(frame, &PORT.text[ROW_ARTIST]);
    face_line(canvas, frame, &artist, frame->artist);
    FaceLine album = text_line(frame, &PORT.text[ROW_ALBUM]);
    bool lyric = frame->lyric != NULL && frame->lyric[0] != '\0';
    album.color = lyric ? album.emphasis : album.color;
    face_line(canvas, frame, &album, lyric ? frame->lyric : frame->album);
    portrait_format(canvas, frame);
    FaceLine modes = text_line(frame, &PORT.text[ROW_MODES]);
    still_text(canvas, &modes, frame->modes, modes.color);
}

ECHO_NP_FACES_CODE static void portrait(Canvas *canvas, const NowPlayingFrame *frame)
{
    Point origin = {12, frame->area.y + 31};
    stereo_dial(canvas, frame, origin);
    badge(canvas, frame, (Point){65, origin.y + 16});
    portrait_text(canvas, frame);
    transport(canvas, frame, &PORT);
}

ECHO_NP_FACES_CODE void face_vu_draw(const NowPlayingFrame *frame, Canvas *canvas)
{
    canvas_fill_rect(canvas, frame->area, VU_PLATE);
    if (frame->area.width > frame->area.height) {
        landscape(canvas, frame);
    } else {
        portrait(canvas, frame);
    }
}

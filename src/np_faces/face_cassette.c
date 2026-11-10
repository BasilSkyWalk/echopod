#include "core/resident.h"
#include "face_text.h"
#include "faces.h"
#include "np_faces/face_shapes.h"
#include "ui/label.h"

enum {
    DEEP = COLOR_RGB(24, 20, 24),
    SHELL = COLOR_RGB(72, 72, 80),
    SHELL_HI = COLOR_RGB(96, 96, 104),
    TAPE = COLOR_RGB(120, 76, 48),
    LABEL = COLOR_RGB(240, 228, 200),
    INK = COLOR_RGB(40, 32, 32),
    INK_SOFT = COLOR_RGB(96, 80, 72),
    RED = COLOR_RGB(176, 48, 32),
    AMBER = COLOR_RGB(232, 152, 40),
    RULE = COLOR_RGB(184, 168, 136),
    SHELL_RADIUS = 6,
    FRAME_RADIUS = 5,
    INSIDE_RADIUS = 4,
    CARD_HEIGHT = 217,
    CARD_OFFSET = 108,
    TEETH = 3,
    HALF = 2,
    WHOLE_PIXEL = ~(SHAPE_Q4 - 1)
};

typedef struct Box {
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
} Box;

typedef struct Spot {
    int16_t x;
    int16_t y;
} Spot;

typedef struct Layout {
    Box shell;
    Box highlight;
    Box label;
    Box rules[2];
    Box bands[3];
    Box letter;
    Box codec;
    Box frame;
    Box inside;
    Box icon;
    Box trap;
    Box opening;
    Box tape;
    Spot screws[4];
    int16_t reels[2];
    int16_t holes[2];
    int16_t reel_y;
    int16_t hole_y;
    int16_t screw;
    int16_t slot;
    int16_t hub;
    int16_t hole;
    int16_t tooth_radius;
    int16_t tooth_distance;
    int16_t pack_min;
    int16_t pack_max;
    int16_t hole_hub;
    int16_t inset;
    Color letter_color;
    Color codec_color;
    uint8_t letter_align;
    uint8_t codec_align;
} Layout;

typedef struct Pen {
    Canvas *canvas;
    const Layout *layout;
    int top;
} Pen;

typedef struct Reel {
    Point centre;
    int pack;
    uint8_t angle;
} Reel;

typedef struct Ink {
    const Font *font;
    Color color;
    FaceAlign align;
} Ink;

ECHO_NP_FACES_DATA static const char LETTER[] = "A";
ECHO_NP_FACES_DATA static const uint8_t TOOTH_STEPS[TEETH] = {0, 85, 171};

ECHO_NP_FACES_DATA static const Layout LANDSCAPE = {
    .shell = {11, 27, 297, 140},
    .highlight = {17, 28, 285, 1},
    .label = {25, 32, 269, 92},
    .rules = {{33, 54, 253, 1}, {33, 77, 253, 1}},
    .bands = {{25, 80, 269, 4}, {25, 84, 269, 31}, {25, 115, 269, 4}},
    .letter = {25, 84, 40, 31},
    .codec = {254, 84, 40, 31},
    .frame = {65, 80, 189, 39},
    .inside = {67, 82, 185, 35},
    .icon = {151, 90, 18, 18},
    .trap = {71, 129, 177, 38},
    .opening = {87, 153, 145, 14},
    .tape = {87, 157, 145, 3},
    .screws = {{18, 34}, {300, 34}, {18, 159}, {300, 159}},
    .reels = {99, 219},
    .holes = {103, 215},
    .reel_y = 99,
    .hole_y = 143,
    .screw = 56,
    .slot = 5,
    .hub = 168,
    .hole = 104,
    .tooth_radius = 32,
    .tooth_distance = 88,
    .pack_min = 200,
    .pack_max = 424,
    .hole_hub = 72,
    .inset = 12,
    .letter_color = LABEL,
    .codec_color = LABEL,
    .letter_align = FACE_ALIGN_CENTRE,
    .codec_align = FACE_ALIGN_CENTRE,
};

ECHO_NP_FACES_DATA static const Layout PORTRAIT = {
    .shell = {8, 0, 153, 100},
    .highlight = {14, 1, 141, 1},
    .label = {20, 5, 129, 63},
    .rules = {{25, 28, 119, 1}},
    .bands = {{20, 31, 129, 3}, {20, 34, 129, 27}, {20, 61, 129, 3}},
    .letter = {25, 6, 14, 20},
    .codec = {100, 10, 44, 16},
    .frame = {34, 30, 101, 35},
    .inside = {36, 32, 97, 31},
    .icon = {76, 38, 18, 18},
    .trap = {34, 72, 101, 28},
    .opening = {46, 89, 77, 11},
    .tape = {46, 92, 77, 3},
    .screws = {{14, 6}, {154, 6}, {14, 92}, {154, 92}},
    .reels = {58, 110},
    .holes = {58, 110},
    .reel_y = 47,
    .hole_y = 82,
    .screw = 40,
    .slot = 3,
    .hub = 136,
    .hole = 88,
    .tooth_radius = 24,
    .tooth_distance = 72,
    .pack_min = 152,
    .pack_max = 312,
    .hole_hub = 56,
    .inset = 8,
    .letter_color = RED,
    .codec_color = INK_SOFT,
    .letter_align = FACE_ALIGN_LEFT,
    .codec_align = FACE_ALIGN_RIGHT,
};

ECHO_NP_FACES_CODE static bool present(const char *text)
{
    return text != NULL && text[0] != '\0';
}

ECHO_NP_FACES_CODE static Rect rect_at(Box box, int top)
{
    return (Rect){box.x, box.y + top, box.width, box.height};
}

ECHO_NP_FACES_CODE static void paint(const Pen *pen, Box box, Color color)
{
    canvas_fill_rect(pen->canvas, rect_at(box, pen->top), color);
}

ECHO_NP_FACES_CODE static Point centre_q4(int x, int y)
{
    return (Point){x * SHAPE_Q4 + SHAPE_Q4 / HALF, y * SHAPE_Q4 + SHAPE_Q4 / HALF};
}

ECHO_NP_FACES_CODE static void disc(Canvas *canvas, Point centre, int radius, Color color)
{
    shape_ring(canvas, (ShapeRing){(int16_t)centre.x, (int16_t)centre.y, (int16_t)radius, 0},
               color);
}

ECHO_NP_FACES_CODE static void put(Canvas *canvas, Ink ink, Rect box, const char *text)
{
    Rect fitted = face_fitted_area(ink.font, box, text, ink.align);
    label_draw(canvas, (TextStyle){ink.font, ink.color}, (Label){fitted, text});
}

ECHO_NP_FACES_CODE static void draw_screws(const Pen *pen)
{
    const Layout *layout = pen->layout;
    for (int index = 0; index < 4; index++) {
        int x = layout->screws[index].x;
        int y = layout->screws[index].y + pen->top;
        disc(pen->canvas, centre_q4(x, y), layout->screw, RULE);
        canvas_fill_rect(pen->canvas, (Rect){x - layout->slot / HALF, y, layout->slot, 1}, DEEP);
    }
}

ECHO_NP_FACES_CODE static void draw_shell(const Pen *pen)
{
    const Layout *layout = pen->layout;
    shape_rounded(pen->canvas, rect_at(layout->shell, pen->top), SHELL_RADIUS, SHELL);
    paint(pen, layout->highlight, SHELL_HI);
    draw_screws(pen);
}

ECHO_NP_FACES_CODE static void draw_letters(const Pen *pen, const NowPlayingFrame *frame)
{
    const Layout *layout = pen->layout;
    const ThemeFonts *fonts = &frame->theme->fonts;
    Ink letter = {fonts->title, layout->letter_color, (FaceAlign)layout->letter_align};
    put(pen->canvas, letter, rect_at(layout->letter, pen->top), LETTER);
    if (present(frame->codec)) {
        Ink codec = {fonts->small, layout->codec_color, (FaceAlign)layout->codec_align};
        put(pen->canvas, codec, rect_at(layout->codec, pen->top), frame->codec);
    }
}

ECHO_NP_FACES_CODE static void draw_label(const Pen *pen, const NowPlayingFrame *frame)
{
    const Layout *layout = pen->layout;
    paint(pen, layout->label, LABEL);
    for (int index = 0; index < HALF; index++) {
        paint(pen, layout->rules[index], RULE);
    }
    for (int index = 0; index < TEETH; index++) {
        paint(pen, layout->bands[index], index == 1 ? RED : AMBER);
    }
    draw_letters(pen, frame);
}

ECHO_NP_FACES_CODE static void draw_reel(Canvas *window, const Layout *layout, const Reel *reel)
{
    disc(window, reel->centre, reel->pack, TAPE);
    disc(window, reel->centre, layout->hub, LABEL);
    disc(window, reel->centre, layout->hole, DEEP);
    for (int index = 0; index < TEETH; index++) {
        uint8_t angle = (uint8_t)(reel->angle + TOOTH_STEPS[index]);
        Point at = {reel->centre.x + layout->tooth_distance * shape_sine(angle) / SHAPE_UNIT,
                    reel->centre.y - layout->tooth_distance * shape_cosine(angle) / SHAPE_UNIT};
        disc(window, at, layout->tooth_radius, LABEL);
    }
}

ECHO_NP_FACES_CODE static void draw_reels(const Pen *pen, const NowPlayingFrame *frame)
{
    const Layout *layout = pen->layout;
    int high = layout->pack_max;
    int low = layout->pack_min;
    int played =
        face_progress_width(frame->shown_ms, frame->status->duration_ms, high * high - low * low);
    int packs[2] = {(int)shape_isqrt((uint32_t)(high * high - played)) & WHOLE_PIXEL,
                    (int)shape_isqrt((uint32_t)(low * low + played)) & WHOLE_PIXEL};
    Canvas window = canvas_clipped(pen->canvas, rect_at(layout->inside, pen->top));
    Reel reel = {.angle = (uint8_t)(0 - frame->tick * 8)};
    for (int index = 0; index < HALF; index++) {
        reel.centre = centre_q4(layout->reels[index], layout->reel_y + pen->top);
        reel.pack = packs[index];
        draw_reel(&window, layout, &reel);
    }
}

ECHO_NP_FACES_CODE static void draw_window(const Pen *pen, const NowPlayingFrame *frame)
{
    const Layout *layout = pen->layout;
    shape_rounded(pen->canvas, rect_at(layout->frame, pen->top), FRAME_RADIUS, SHELL);
    shape_rounded(pen->canvas, rect_at(layout->inside, pen->top), INSIDE_RADIUS, DEEP);
    draw_reels(pen, frame);
    FaceIcon icon = face_badge_kind(frame);
    if (icon == FACE_ICON_NONE && !frame->playing) {
        icon = FACE_ICON_PAUSE;
    }
    face_icon_draw(pen->canvas, icon, rect_at(layout->icon, pen->top), LABEL);
}

ECHO_NP_FACES_CODE static void draw_head(const Pen *pen, const NowPlayingFrame *frame)
{
    const Layout *layout = pen->layout;
    shape_trapezoid(pen->canvas, rect_at(layout->trap, pen->top), layout->inset, SHELL_HI);
    for (int index = 0; index < HALF; index++) {
        Point centre = centre_q4(layout->holes[index], layout->hole_y + pen->top);
        disc(pen->canvas, centre, layout->hole_hub, DEEP);
    }
    paint(pen, layout->opening, DEEP);
    FaceBar bar = {rect_at(layout->tape, pen->top), TAPE, AMBER, LABEL};
    face_bar_draw(pen->canvas, frame, &bar);
}

ECHO_NP_FACES_CODE static void draw_text(Canvas *canvas, const NowPlayingFrame *frame)
{
    const ThemeFonts *fonts = &frame->theme->fonts;
    FaceLine title = {fonts->title, {33, 33, 253, 20}, INK, INK, FACE_ALIGN_LEFT};
    face_line(canvas, frame, &title, frame->song);
    FaceLine info = {fonts->body, {33, 56, 253, 20}, INK, INK_SOFT, FACE_ALIGN_LEFT};
    face_feedback_line(canvas, frame, &info, frame->artist);
    FaceLine times = {fonts->small, {17, 129, 286, 16}, LABEL, LABEL, FACE_ALIGN_LEFT};
    face_times_draw(canvas, frame, &times);
}

ECHO_NP_FACES_CODE static void draw_format(Canvas *canvas, const NowPlayingFrame *frame, int top)
{
    char buffer[FACE_FEEDBACK_CAPACITY];
    const char *feedback = face_feedback_text(frame, buffer, sizeof buffer);
    const char *text = feedback != NULL ? feedback : frame->stream;
    if (present(text)) {
        Ink ink = {frame->theme->fonts.small, feedback != NULL ? INK : INK_SOFT, FACE_ALIGN_CENTRE};
        put(canvas, ink, (Rect){16, top, 137, 16}, text);
    }
}

ECHO_NP_FACES_CODE static void draw_card_text(Canvas *canvas, const NowPlayingFrame *frame, int c)
{
    const ThemeFonts *fonts = &frame->theme->fonts;
    bool lyric = present(frame->lyric);
    FaceLine title = {fonts->title, {16, c + 3, 137, 20}, INK, INK, FACE_ALIGN_CENTRE};
    face_line(canvas, frame, &title, frame->song);
    FaceLine artist = {fonts->body, {16, c + 26, 137, 20}, INK, INK_SOFT, FACE_ALIGN_CENTRE};
    face_line(canvas, frame, &artist, frame->artist);
    FaceLine album = {
        fonts->body, {16, c + 49, 137, 20}, INK, lyric ? INK : INK_SOFT, FACE_ALIGN_CENTRE};
    face_line(canvas, frame, &album, lyric ? frame->lyric : frame->album);
    draw_format(canvas, frame, c + 73);
    FaceLine times = {fonts->small, {16, c + 90, 137, 16}, INK, INK, FACE_ALIGN_LEFT};
    face_times_draw(canvas, frame, &times);
}

ECHO_NP_FACES_CODE static void draw_card(Canvas *canvas, const NowPlayingFrame *frame, int c)
{
    canvas_fill_rect(canvas, (Rect){8, c, 153, 109}, LABEL);
    canvas_fill_rect(canvas, (Rect){8, c, 153, 2}, RED);
    for (int row = 24; row <= 70; row += 23) {
        canvas_fill_rect(canvas, (Rect){16, c + row, 137, 1}, RULE);
    }
    draw_card_text(canvas, frame, c);
}

ECHO_NP_FACES_CODE void face_cassette_draw(const NowPlayingFrame *frame, Canvas *canvas)
{
    bool landscape = frame->area.width > frame->area.height;
    int top = landscape ? 0 : frame->area.y + (frame->area.height - CARD_HEIGHT) / HALF;
    Pen pen = {canvas, landscape ? &LANDSCAPE : &PORTRAIT, top};
    canvas_fill_rect(canvas, frame->area, DEEP);
    draw_shell(&pen);
    draw_label(&pen, frame);
    draw_window(&pen, frame);
    draw_head(&pen, frame);
    if (landscape) {
        draw_text(canvas, frame);
    } else {
        draw_card(canvas, frame, top + CARD_OFFSET);
    }
}

#include "np_faces/face_shapes.h"

#include "core/resident.h"

enum {
    QUARTER_STEPS = 64,
    QUARTER_SHIFT = 6,
    QUARTER_MASK = 63,
    ODD_QUARTER = 1,
    NEGATIVE_QUARTER = 2,
    QUARTER_TURN = 64,
    HALF_PIXEL_Q4 = 8,
    FRACTION_MASK = 15,
    PIXEL_SHIFT = 4,
    SQRT_TOP_BIT = 1 << 30,
    SPOKE_FAR = 1 << 30,
    SPOKE_Q4_UNIT = SHAPE_Q4 * SHAPE_UNIT,
    LATTICE_OFFSET = 2,
    DIGIT_SEGMENTS = 10,
    BAR_COUNT = 3,
    POST_COUNT = 4,
    COLON_DOTS = 2
};

ECHO_NP_FACES_DATA static const uint8_t QUARTER[QUARTER_STEPS + 1] = {
    0,   6,   13,  19,  25,  31,  37,  44,  50,  56,  62,  68,  74,  80,  86,  92,  98,
    103, 109, 115, 120, 126, 131, 136, 142, 147, 152, 157, 162, 167, 171, 176, 180, 185,
    189, 193, 197, 201, 205, 208, 212, 215, 219, 222, 225, 228, 231, 233, 236, 238, 240,
    242, 244, 246, 247, 249, 250, 251, 252, 253, 254, 254, 255, 255, 255};

ECHO_NP_FACES_DATA static const uint8_t DIGIT_MASKS[DIGIT_SEGMENTS] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

ECHO_NP_FACES_DATA static const uint8_t BAR_BITS[BAR_COUNT] = {0, 6, 3};
ECHO_NP_FACES_DATA static const uint8_t POST_BITS[POST_COUNT] = {5, 1, 4, 2};

typedef struct Bounds {
    int low;
    int high;
} Bounds;

typedef struct DigitPen {
    Canvas *canvas;
    const SegmentStyle *style;
    Point origin;
} DigitPen;

typedef struct DigitFrame {
    int left;
    int right;
    int top;
    int middle;
    int bottom;
    int half;
} DigitFrame;

typedef struct Run {
    int left;
    int right;
    int y;
} Run;

typedef struct InsetRow {
    int row;
    int inset;
} InsetRow;

typedef struct Post {
    int x;
    int top;
    int bottom;
} Post;

ECHO_NP_FACES_CODE static int round_div(int numerator, int denominator)
{
    int half = denominator / 2;
    return (numerator + (numerator >= 0 ? half : -half)) / denominator;
}

ECHO_NP_FACES_CODE static int magnitude(int value)
{
    return value < 0 ? -value : value;
}

ECHO_NP_FACES_CODE static int smaller(int first, int second)
{
    return first < second ? first : second;
}

ECHO_NP_FACES_CODE static int larger(int first, int second)
{
    return first > second ? first : second;
}

ECHO_NP_FACES_CODE int shape_sine(uint8_t angle)
{
    unsigned quadrant = (unsigned)angle >> QUARTER_SHIFT;
    unsigned step = (unsigned)angle & QUARTER_MASK;
    int value = QUARTER[(quadrant & ODD_QUARTER) != 0 ? QUARTER_STEPS - step : step];
    return quadrant >= NEGATIVE_QUARTER ? -value : value;
}

ECHO_NP_FACES_CODE int shape_cosine(uint8_t angle)
{
    return shape_sine((uint8_t)(angle + QUARTER_TURN));
}

ECHO_NP_FACES_CODE uint32_t shape_isqrt(uint32_t value)
{
    uint32_t root = 0;
    uint32_t bit = SQRT_TOP_BIT;
    while (bit > value) {
        bit >>= 2;
    }
    for (; bit != 0; bit >>= 2) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
    }
    return root;
}

ECHO_NP_FACES_CODE Point shape_polar(Point centre, uint8_t angle, int radius)
{
    return (Point){centre.x + round_div(radius * shape_sine(angle), SHAPE_UNIT),
                   centre.y - round_div(radius * shape_cosine(angle), SHAPE_UNIT)};
}

ECHO_NP_FACES_CODE static void blend_at(Canvas *canvas, Point point, Color color, uint8_t alpha)
{
    Rect clip = canvas->clip;
    bool inside = point.x >= clip.x && point.x < clip.x + clip.width && point.y >= clip.y &&
                  point.y < clip.y + clip.height;
    if (alpha == 0 || !inside) {
        return;
    }
    int offset = (point.y - canvas->bounds.y) * canvas->bounds.width + point.x - canvas->bounds.x;
    canvas->pixels[offset] = color_blend(canvas->pixels[offset], color, alpha);
}

ECHO_NP_FACES_CODE static uint8_t coverage(int fraction_q4)
{
    return (uint8_t)(fraction_q4 * COLOR_ALPHA_OPAQUE / SHAPE_Q4);
}

ECHO_NP_FACES_CODE void shape_span(Canvas *canvas, ShapeSpan span, Color color)
{
    Rect clip = canvas->clip;
    if (span.right_q4 <= span.left_q4 || span.y < clip.y || span.y >= clip.y + clip.height) {
        return;
    }
    int left = span.left_q4 >> PIXEL_SHIFT;
    int right = span.right_q4 >> PIXEL_SHIFT;
    int left_part = span.left_q4 & FRACTION_MASK;
    int right_part = span.right_q4 & FRACTION_MASK;
    if (left == right) {
        blend_at(canvas, (Point){left, span.y}, color, coverage(span.right_q4 - span.left_q4));
        return;
    }
    if (left_part != 0) {
        blend_at(canvas, (Point){left, span.y}, color, coverage(SHAPE_Q4 - left_part));
        left++;
    }
    canvas_fill_rect(canvas, (Rect){left, span.y, right - left, 1}, color);
    blend_at(canvas, (Point){right, span.y}, color, coverage(right_part));
}

ECHO_NP_FACES_CODE static ShapeSpan span_of(int y, int left_q4, int right_q4)
{
    return (ShapeSpan){(int16_t)y, (int16_t)left_q4, (int16_t)right_q4};
}

ECHO_NP_FACES_CODE static void ring_row(Canvas *canvas, const ShapeRing *ring, int y, Color color)
{
    int dy = y * SHAPE_Q4 + HALF_PIXEL_Q4 - ring->y_q4;
    int outer = ring->outer_q4;
    int inner = ring->inner_q4;
    if (dy * dy >= outer * outer) {
        return;
    }
    int reach = (int)shape_isqrt((uint32_t)(outer * outer - dy * dy));
    int centre = ring->x_q4;
    if (dy * dy >= inner * inner) {
        shape_span(canvas, span_of(y, centre - reach, centre + reach), color);
        return;
    }
    int hole = (int)shape_isqrt((uint32_t)(inner * inner - dy * dy));
    shape_span(canvas, span_of(y, centre - reach, centre - hole), color);
    shape_span(canvas, span_of(y, centre + hole, centre + reach), color);
}

ECHO_NP_FACES_CODE void shape_ring(Canvas *canvas, ShapeRing ring, Color color)
{
    int top = larger((ring.y_q4 - ring.outer_q4) >> PIXEL_SHIFT, canvas->clip.y);
    int bottom = smaller(((ring.y_q4 + ring.outer_q4) >> PIXEL_SHIFT) + 1,
                         canvas->clip.y + canvas->clip.height);
    for (int y = top; y < bottom; y++) {
        ring_row(canvas, &ring, y, color);
    }
}

ECHO_NP_FACES_CODE static int corner_inset(int radius, int edge_distance)
{
    int offset = (2 * (radius - edge_distance) - 1) * HALF_PIXEL_Q4;
    int radius_q4 = radius * SHAPE_Q4;
    return radius_q4 - (int)shape_isqrt((uint32_t)(radius_q4 * radius_q4 - offset * offset));
}

ECHO_NP_FACES_CODE static void inset_row(Canvas *canvas, Rect area, InsetRow at, Color color)
{
    int left = area.x * SHAPE_Q4 + at.inset;
    int right = (area.x + area.width) * SHAPE_Q4 - at.inset;
    shape_span(canvas, span_of(area.y + at.row, left, right), color);
}

ECHO_NP_FACES_CODE void shape_rounded(Canvas *canvas, Rect area, int radius, Color color)
{
    int first = larger(0, canvas->clip.y - area.y);
    int last = smaller(area.height, canvas->clip.y + canvas->clip.height - area.y);
    for (int row = first; row < last; row++) {
        int edge = smaller(row, area.height - 1 - row);
        int inset = edge < radius ? corner_inset(radius, edge) : 0;
        inset_row(canvas, area, (InsetRow){row, inset}, color);
    }
}

ECHO_NP_FACES_CODE void shape_trapezoid(Canvas *canvas, Rect area, int top_inset, Color color)
{
    int first = larger(0, canvas->clip.y - area.y);
    int last = smaller(area.height, canvas->clip.y + canvas->clip.height - area.y);
    for (int row = first; row < last; row++) {
        int inset = top_inset * SHAPE_Q4 * (2 * (area.height - row) - 1) / (2 * area.height);
        inset_row(canvas, area, (InsetRow){row, inset}, color);
    }
}

ECHO_NP_FACES_CODE static Bounds axis_bounds(int factor, int low, int high, Bounds within)
{
    if (factor == 0) {
        bool outside = low > 0 || high < 0;
        return outside ? (Bounds){within.low, within.low} : within;
    }
    if (factor < 0) {
        int negated_low = -high;
        high = -low;
        low = negated_low;
        factor = -factor;
    }
    return (Bounds){larger(within.low, round_div(low, factor)),
                    smaller(within.high, round_div(high, factor))};
}

ECHO_NP_FACES_CODE static Bounds spoke_bounds(const ShapeSpoke *spoke, int y)
{
    int sine = shape_sine(spoke->angle);
    int cosine = shape_cosine(spoke->angle);
    int row = (y - spoke->pivot.y) * SHAPE_Q4;
    int half = spoke->width_q4 * SHAPE_UNIT / 2;
    Bounds along =
        axis_bounds(sine, spoke->from * SPOKE_Q4_UNIT + row * cosine,
                    spoke->to * SPOKE_Q4_UNIT + row * cosine, (Bounds){-SPOKE_FAR, SPOKE_FAR});
    return axis_bounds(cosine, -half - row * sine, half - row * sine, along);
}

ECHO_NP_FACES_CODE void shape_spoke(Canvas *canvas, const ShapeSpoke *spoke, Color color)
{
    int reach =
        larger(magnitude(spoke->from), magnitude(spoke->to)) + spoke->width_q4 / SHAPE_Q4 + 1;
    int top = larger(spoke->pivot.y - reach, canvas->clip.y);
    int bottom = smaller(spoke->pivot.y + reach + 1, canvas->clip.y + canvas->clip.height);
    int origin = spoke->pivot.x * SHAPE_Q4 + HALF_PIXEL_Q4;
    for (int y = top; y < bottom; y++) {
        Bounds row = spoke_bounds(spoke, y);
        if (row.high > row.low) {
            shape_span(canvas, span_of(y, origin + row.low, origin + row.high), color);
        }
    }
}

ECHO_NP_FACES_CODE static void run_draw(const DigitPen *pen, Run run, Color color)
{
    if (run.right < run.left) {
        return;
    }
    const SegmentStyle *style = pen->style;
    int lean = style->lean;
    int shift = lean == 0 ? 0 : (pen->origin.y + style->height - 1 - run.y) / lean;
    canvas_fill_rect(pen->canvas, (Rect){run.left + shift, run.y, run.right - run.left + 1, 1},
                     color);
}

ECHO_NP_FACES_CODE static DigitFrame digit_frame(const SegmentStyle *style, Point origin)
{
    int half = style->thickness / 2;
    return (DigitFrame){.left = origin.x + half,
                        .right = origin.x + style->width - 1 - half,
                        .top = origin.y + half,
                        .middle = origin.y + (style->height - 1) / 2,
                        .bottom = origin.y + style->height - 1 - half,
                        .half = half};
}

ECHO_NP_FACES_CODE static Color segment_color(const SegmentStyle *style, uint8_t mask, int bit)
{
    return ((mask >> bit) & 1) != 0 ? style->lit : style->unlit;
}

ECHO_NP_FACES_CODE static void digit_bar(const DigitPen *pen, const DigitFrame *frame, int row,
                                         Color color)
{
    for (int offset = -frame->half; offset <= frame->half; offset++) {
        int slant = magnitude(offset) + 1;
        run_draw(pen, (Run){frame->left + slant, frame->right - slant, row + offset}, color);
    }
}

ECHO_NP_FACES_CODE static void digit_post(const DigitPen *pen, const DigitFrame *frame, Post post,
                                          Color color)
{
    for (int row = post.top; row <= post.bottom; row++) {
        int reach = smaller(frame->half, smaller(row - post.top, post.bottom - row) - 1);
        if (reach >= 0) {
            run_draw(pen, (Run){post.x - reach, post.x + reach, row}, color);
        }
    }
}

ECHO_NP_FACES_CODE static void digit_bars(const DigitPen *pen, const DigitFrame *frame,
                                          uint8_t mask)
{
    const int rows[BAR_COUNT] = {frame->top, frame->middle, frame->bottom};
    for (int index = 0; index < BAR_COUNT; index++) {
        digit_bar(pen, frame, rows[index], segment_color(pen->style, mask, BAR_BITS[index]));
    }
}

ECHO_NP_FACES_CODE static void digit_posts(const DigitPen *pen, const DigitFrame *frame,
                                           uint8_t mask)
{
    const Post posts[POST_COUNT] = {{frame->left, frame->top, frame->middle},
                                    {frame->right, frame->top, frame->middle},
                                    {frame->left, frame->middle, frame->bottom},
                                    {frame->right, frame->middle, frame->bottom}};
    for (int index = 0; index < POST_COUNT; index++) {
        digit_post(pen, frame, posts[index], segment_color(pen->style, mask, POST_BITS[index]));
    }
}

ECHO_NP_FACES_CODE void shape_digit(Canvas *canvas, const SegmentStyle *style, Point origin,
                                    uint8_t mask)
{
    DigitPen pen = {canvas, style, origin};
    DigitFrame frame = digit_frame(style, origin);
    digit_bars(&pen, &frame, mask);
    digit_posts(&pen, &frame, mask);
}

ECHO_NP_FACES_CODE void shape_colon(Canvas *canvas, const SegmentStyle *style, Point origin,
                                    bool lit)
{
    DigitPen pen = {canvas, style, origin};
    DigitFrame frame = digit_frame(style, origin);
    int size = style->thickness - 1;
    int centres[COLON_DOTS] = {(frame.top + frame.middle) / 2,
                               (frame.middle + frame.bottom) / 2 + 1};
    for (int dot = 0; dot < COLON_DOTS; dot++) {
        int first = centres[dot] - size / 2;
        for (int row = first; row < first + size; row++) {
            run_draw(&pen, (Run){origin.x + 2, origin.x + 1 + size, row},
                     lit ? style->lit : style->unlit);
        }
    }
}

ECHO_NP_FACES_CODE uint8_t shape_digit_mask(unsigned digit)
{
    return digit < DIGIT_SEGMENTS ? DIGIT_MASKS[digit] : 0;
}

ECHO_NP_FACES_CODE void shape_lattice(Canvas *canvas, Rect area, int pitch, Color color)
{
    if (pitch <= 0) {
        return;
    }
    for (int x = area.x + LATTICE_OFFSET; x < area.x + area.width; x += pitch) {
        canvas_fill_rect(canvas, (Rect){x, area.y, 1, area.height}, color);
    }
    for (int y = area.y + LATTICE_OFFSET; y < area.y + area.height; y += pitch) {
        canvas_fill_rect(canvas, (Rect){area.x, y, area.width, 1}, color);
    }
}

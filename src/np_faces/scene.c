#include "np_faces/scene.h"

#include "core/draw_profile.h"
#include "core/resident.h"
#include "face_text.h"
#include "np_faces/face_shapes.h"
#include "ui/backdrop.h"
#include "ui/label.h"

enum {
    IMMEDIATE_BIT = 0x8000,
    SIGN_SHIFT = 17,
    MOST_CLIPS = 4,
    FIRST_TEXT_FIELD = 1,
    TEXT_FIELDS = 7,
    WORD_BITS = 32,
    SHIFT_MASK = 31,
    REFLECTION_LIGHT = 80
};

typedef enum SceneKind {
    SCENE_KIND_CONTROL,
    SCENE_KIND_VALUE,
    SCENE_KIND_DRAW
} SceneKind;

#define SCENE_OP(id, name, kind, count, raw) SCENE_OP_##id,
typedef enum SceneOp {
#include "np_faces/scene_ops.def"
    SCENE_OP_COUNT
} SceneOp;
#undef SCENE_OP

#define SCENE_INPUT(id, name) SCENE_INPUT_##id,
typedef enum SceneInput {
#include "np_faces/scene_inputs.def"
    SCENE_INPUT_COUNT
} SceneInput;
#undef SCENE_INPUT

#define SCENE_SOURCE(id, name) SCENE_SOURCE_##id,
typedef enum SceneSource {
#include "np_faces/scene_sources.def"
    SCENE_SOURCE_COUNT
} SceneSource;
#undef SCENE_SOURCE

_Static_assert((int)SCENE_INPUT_COUNT < (int)SCENE_REGISTERS,
               "the inputs leave registers for the scene");
_Static_assert((int)SCENE_SOURCE_COUNT <= (int)SCENE_ITEM_BASE,
               "the sources come before the menu items");
_Static_assert((int)SCENE_ITEM_BASE < (int)SCENE_STRING_BASE,
               "the menu items come before the strings");

#define SCENE_OP(id, name, kind, count, raw) _Static_assert(count <= SCENE_MOST_OPERANDS, #name);
#include "np_faces/scene_ops.def"
#undef SCENE_OP

#define SCENE_OP(id, name, kind, count, raw) count,
ECHO_NP_FACES_DATA static const uint8_t COUNTS[SCENE_OP_COUNT] = {
#include "np_faces/scene_ops.def"
};
#undef SCENE_OP

#define SCENE_OP(id, name, kind, count, raw) SCENE_KIND_##kind,
ECHO_NP_FACES_DATA static const uint8_t KINDS[SCENE_OP_COUNT] = {
#include "np_faces/scene_ops.def"
};
#undef SCENE_OP

#define SCENE_OP(id, name, kind, count, raw) raw,
ECHO_NP_FACES_DATA static const uint8_t RAW[SCENE_OP_COUNT] = {
#include "np_faces/scene_ops.def"
};
#undef SCENE_OP

typedef struct Machine {
    int32_t registers[SCENE_REGISTERS];
    uint16_t raw[SCENE_MOST_OPERANDS];
    int32_t values[SCENE_MOST_OPERANDS];
    const NowPlayingFrame *frame;
    Canvas canvases[MOST_CLIPS + 1];
    int depth;
    SegmentStyle style;
    FaceTimes times;
    char feedback[FACE_FEEDBACK_CAPACITY];
    const char *feedback_text;
} Machine;

_Static_assert(sizeof(Machine) <= SCENE_WORK_BYTES, "the scene's working memory holds the machine");

ECHO_NP_FACES_CODE static int32_t wrapped(uint32_t value)
{
    return (int32_t)value;
}

ECHO_NP_FACES_CODE static int32_t operand(const Machine *machine, uint16_t raw)
{
    if ((raw & IMMEDIATE_BIT) != 0) {
        return (int32_t)((uint32_t)raw << SIGN_SHIFT) >> SIGN_SHIFT;
    }
    return raw < SCENE_REGISTERS ? machine->registers[raw] : 0;
}

ECHO_NP_FACES_CODE static void store(Machine *machine, int32_t value)
{
    uint16_t target = machine->raw[0];
    if (target < SCENE_REGISTERS && target >= SCENE_INPUT_COUNT) {
        machine->registers[target] = value;
    }
}

ECHO_NP_FACES_CODE static int32_t divided(int32_t first, int32_t second)
{
    if (second == 0 || (first == INT32_MIN && second == -1)) {
        return second == 0 ? 0 : first;
    }
    return first / second;
}

ECHO_NP_FACES_CODE static int32_t remainder_of(int32_t first, int32_t second)
{
    if (second == 0 || (first == INT32_MIN && second == -1)) {
        return 0;
    }
    return first % second;
}

ECHO_NP_FACES_CODE static int32_t arithmetic(SceneOp op, int32_t first, int32_t second)
{
    uint32_t a = (uint32_t)first;
    uint32_t b = (uint32_t)second;
    switch (op) {
    case SCENE_OP_ADD:
        return wrapped(a + b);
    case SCENE_OP_SUB:
        return wrapped(a - b);
    case SCENE_OP_MUL:
        return wrapped(a * b);
    case SCENE_OP_DIV:
        return divided(first, second);
    case SCENE_OP_MOD:
        return remainder_of(first, second);
    case SCENE_OP_AND:
        return wrapped(a & b);
    case SCENE_OP_OR:
        return wrapped(a | b);
    case SCENE_OP_XOR:
        return wrapped(a ^ b);
    case SCENE_OP_SHL:
        return wrapped(a << (b & SHIFT_MASK));
    case SCENE_OP_SHR:
        return first >> (b & SHIFT_MASK);
    case SCENE_OP_USHR:
        return wrapped(a >> (b & SHIFT_MASK));
    case SCENE_OP_LT:
        return first < second;
    case SCENE_OP_LE:
        return first <= second;
    case SCENE_OP_EQ:
        return first == second;
    case SCENE_OP_NE:
        return first != second;
    case SCENE_OP_MIN:
        return first < second ? first : second;
    default:
        return first > second ? first : second;
    }
}

ECHO_NP_FACES_CODE static const char *string_at(const SceneCode *scene, int32_t index)
{
    const char *text = scene->strings;
    uint32_t left = scene->strings_size;
    for (int32_t skipped = 0; text != NULL && left > 0 && skipped < index; skipped++) {
        while (left > 0 && *text != '\0') {
            text++;
            left--;
        }
        if (left > 0) {
            text++;
            left--;
        }
    }
    uint32_t length = 0;
    while (length < left && text[length] != '\0') {
        length++;
    }
    return text != NULL && length < left ? text : NULL;
}

ECHO_NP_FACES_CODE static int32_t item_count(const NowPlayingFrame *frame)
{
    const ListModel *model = frame->menu != NULL ? frame->menu->model : NULL;
    return model != NULL ? model->count(model->context) : 0;
}

ECHO_NP_FACES_CODE static const char *item_label(const NowPlayingFrame *frame, int32_t index)
{
    const ListModel *model = frame->menu != NULL ? frame->menu->model : NULL;
    bool listed = model != NULL && index >= 0 && index < item_count(frame);
    return listed ? model->label(model->context, index) : NULL;
}

ECHO_NP_FACES_CODE static const char *source_text(Machine *machine, int32_t source)
{
    const NowPlayingFrame *frame = machine->frame;
    switch (source) {
    case SCENE_SOURCE_SONG:
        return frame->song;
    case SCENE_SOURCE_ARTIST:
        return frame->artist;
    case SCENE_SOURCE_ALBUM:
        return frame->album;
    case SCENE_SOURCE_LYRIC:
        return frame->lyric;
    case SCENE_SOURCE_CODEC:
        return frame->codec;
    case SCENE_SOURCE_STREAM:
        return frame->stream;
    case SCENE_SOURCE_DETAIL:
        return frame->detail;
    case SCENE_SOURCE_MODES:
        return frame->modes;
    case SCENE_SOURCE_FEEDBACK:
        return machine->feedback_text;
    case SCENE_SOURCE_ELAPSED:
        return machine->times.elapsed;
    case SCENE_SOURCE_REMAINING:
        return machine->times.remaining;
    default:
        if (source >= SCENE_ITEM_BASE && source < SCENE_STRING_BASE) {
            return item_label(frame, source - SCENE_ITEM_BASE);
        }
        return source >= SCENE_STRING_BASE ? string_at(&frame->scene, source - SCENE_STRING_BASE)
                                           : NULL;
    }
}

ECHO_NP_FACES_CODE static bool present(const char *text)
{
    return text != NULL && text[0] != '\0';
}

ECHO_NP_FACES_CODE static const Font *font_of(const Machine *machine, int32_t font)
{
    const ThemeFonts *fonts = &machine->frame->theme->fonts;
    return font == 0 ? fonts->title : font == 2 ? fonts->small : fonts->body;
}

ECHO_NP_FACES_CODE static Rect rect_from(const int32_t *values)
{
    return (Rect){values[0], values[1], values[2], values[3]};
}

ECHO_NP_FACES_CODE static void fill_gradient(Canvas *canvas, Rect area, Color top, Color bottom)
{
    for (int row = 0; row < area.height; row++) {
        Color color = color_between(top, bottom, row, area.height - 1);
        canvas_fill_rect(canvas, (Rect){area.x, area.y + row, area.width, 1}, color);
    }
}

ECHO_NP_FACES_CODE static int32_t compute(Machine *machine, SceneOp op)
{
    const int32_t *v = machine->values;
    const NowPlayingFrame *frame = machine->frame;
    switch (op) {
    case SCENE_OP_SET:
        return v[1];
    case SCENE_OP_CONST:
        return wrapped((uint32_t)machine->raw[1] | (uint32_t)machine->raw[2] << 16);
    case SCENE_OP_NEG:
        return wrapped(0u - (uint32_t)v[1]);
    case SCENE_OP_NOT:
        return v[1] == 0;
    case SCENE_OP_ABS:
        return v[1] < 0 ? wrapped(0u - (uint32_t)v[1]) : v[1];
    case SCENE_OP_SIN:
        return shape_sine((uint8_t)v[1]);
    case SCENE_OP_COS:
        return shape_cosine((uint8_t)v[1]);
    case SCENE_OP_ISQRT:
        return (int32_t)shape_isqrt((uint32_t)v[1]);
    case SCENE_OP_SEGMENTS:
        return shape_digit_mask((unsigned)v[1]);
    case SCENE_OP_PRESENT:
        return present(source_text(machine, v[1]));
    case SCENE_OP_PROGRESS:
        return face_progress_width(frame->shown_ms, frame->status->duration_ms, v[1]);
    case SCENE_OP_VOLUME_WIDTH:
        return face_volume_width(frame->status->volume, v[1]);
    case SCENE_OP_WIDTH: {
        const char *text = source_text(machine, v[1]);
        return present(text) ? text_width(font_of(machine, v[2]), text) : 0;
    }
    case SCENE_OP_SELECT:
        return v[1] != 0 ? v[2] : v[3];
    case SCENE_OP_ITEM:
        return v[1] >= 0 && v[1] < SCENE_STRING_BASE - SCENE_ITEM_BASE ? SCENE_ITEM_BASE + v[1]
                                                                       : -1;
    case SCENE_OP_ITEMS:
        return item_count(frame);
    case SCENE_OP_SELECTED:
        return frame->menu != NULL ? frame->menu->selected : 0;
    default:
        return arithmetic(op, v[1], v[2]);
    }
}

ECHO_NP_FACES_CODE static void round_cover(Canvas *canvas, Image cover, const int32_t *v)
{
    int side = 2 * v[2];
    int left = v[0] - v[2];
    int top = v[1] - v[2];
    Quad quad = {left, side, top, side, top, side};
    for (int column = 0; column < side; column++) {
        int across = 2 * column + 1 - side;
        int half = (int)shape_isqrt((uint32_t)(side * side - across * across)) / 2;
        Canvas strip = canvas_clipped(canvas, (Rect){left + column, v[1] - half, 1, 2 * half});
        image_draw_quad(&strip, cover, quad);
    }
}

ECHO_NP_FACES_CODE static void write_text(Machine *machine, Canvas *canvas, bool scrolls)
{
    const int32_t *v = machine->values;
    const char *text = source_text(machine, v[0]);
    if (!present(text)) {
        return;
    }
    const Font *font = font_of(machine, v[1]);
    FaceLine line = {font, rect_from(v + 2), (Color)v[7], (Color)v[7], (FaceAlign)v[6]};
    if (scrolls) {
        face_line(canvas, machine->frame, &line, text);
        return;
    }
    Rect fitted = face_fitted_area(font, line.area, text, line.align);
    label_draw(canvas, (TextStyle){font, line.color}, (Label){fitted, text});
}

ECHO_NP_FACES_CODE static void clip(Machine *machine)
{
    if (machine->depth < MOST_CLIPS) {
        Canvas *current = &machine->canvases[machine->depth];
        machine->canvases[machine->depth + 1] = canvas_clipped(current, rect_from(machine->values));
        machine->depth++;
    }
}

ECHO_NP_FACES_CODE static FaceLine line_of(const Machine *machine, int32_t font,
                                           const int32_t *area, Color emphasis, Color color,
                                           int32_t align)
{
    return (FaceLine){font_of(machine, font), rect_from(area), emphasis, color, (FaceAlign)align};
}

ECHO_NP_FACES_CODE static void draw_text_op(Machine *machine, SceneOp op, Canvas *canvas)
{
    const int32_t *v = machine->values;
    const NowPlayingFrame *frame = machine->frame;
    if (op == SCENE_OP_PLAIN) {
        const char *text = source_text(machine, v[0]);
        if (present(text)) {
            label_draw(canvas, (TextStyle){font_of(machine, v[1]), (Color)v[6]},
                       (Label){rect_from(v + 2), text});
        }
    } else if (op == SCENE_OP_TIMES) {
        FaceLine line = line_of(machine, v[0], v + 1, (Color)v[5], (Color)v[6], FACE_ALIGN_LEFT);
        face_times_draw(canvas, frame, &line);
    } else if (op == SCENE_OP_FORMAT) {
        FaceLine line = line_of(machine, v[0], v + 1, (Color)v[6], (Color)v[6], v[5]);
        face_format_row(canvas, frame, &line);
    } else if (op == SCENE_OP_CODEC) {
        FaceLine line = line_of(machine, v[0], v + 1, (Color)v[5], (Color)v[5], FACE_ALIGN_LEFT);
        face_codec_draw(canvas, &line, frame->codec);
    } else {
        write_text(machine, canvas, op == SCENE_OP_LINE);
    }
}

ECHO_NP_FACES_CODE static void draw(Machine *machine, SceneOp op)
{
    const int32_t *v = machine->values;
    Canvas *canvas = &machine->canvases[machine->depth];
    switch (op) {
    case SCENE_OP_RECT:
        canvas_fill_rect(canvas, rect_from(v), (Color)v[4]);
        break;
    case SCENE_OP_ROUNDED:
        shape_rounded(canvas, rect_from(v), v[4], (Color)v[5]);
        break;
    case SCENE_OP_RING:
        shape_ring(canvas, (ShapeRing){(int16_t)v[0], (int16_t)v[1], (int16_t)v[2], (int16_t)v[3]},
                   (Color)v[4]);
        break;
    case SCENE_OP_SPOKE: {
        ShapeSpoke spoke = {
            {v[0], v[1]}, (uint8_t)v[2], (uint8_t)v[3], (int16_t)v[4], (int16_t)v[5]};
        shape_spoke(canvas, &spoke, (Color)v[6]);
        break;
    }
    case SCENE_OP_TRAPEZOID:
        shape_trapezoid(canvas, rect_from(v), v[4], (Color)v[5]);
        break;
    case SCENE_OP_GRADIENT:
        fill_gradient(canvas, rect_from(v), (Color)v[4], (Color)v[5]);
        break;
    case SCENE_OP_LATTICE:
        shape_lattice(canvas, rect_from(v), v[4], (Color)v[5]);
        break;
    case SCENE_OP_SEGMENT_STYLE:
        machine->style = (SegmentStyle){(uint8_t)v[0], (uint8_t)v[1], (uint8_t)v[2],
                                        (uint8_t)v[3], (Color)v[4],   (Color)v[5]};
        break;
    case SCENE_OP_DIGIT:
        shape_digit(canvas, &machine->style, (Point){v[0], v[1]}, (uint8_t)v[2]);
        break;
    case SCENE_OP_COLON:
        shape_colon(canvas, &machine->style, (Point){v[0], v[1]}, v[2] != 0);
        break;
    case SCENE_OP_CLIP:
        clip(machine);
        break;
    case SCENE_OP_UNCLIP:
        machine->depth = machine->depth > 0 ? machine->depth - 1 : 0;
        break;
    case SCENE_OP_COVER:
        if (machine->frame->cover.pixels != NULL) {
            image_draw_quad(canvas, machine->frame->cover,
                            (Quad){v[0], v[2], v[1], v[2], v[1], v[2]});
        }
        break;
    case SCENE_OP_ROUND_COVER:
        if (machine->frame->cover.pixels != NULL) {
            round_cover(canvas, machine->frame->cover, v);
        }
        break;
    case SCENE_OP_BAR: {
        FaceBar bar = {rect_from(v), (Color)v[4], (Color)v[5], (Color)v[6]};
        face_bar_draw(canvas, machine->frame, &bar);
        break;
    }
    case SCENE_OP_ICON:
        face_icon_draw(canvas, (FaceIcon)v[0], rect_from(v + 1), (Color)v[5]);
        break;
    case SCENE_OP_OUTLINE:
        face_outline(canvas, rect_from(v), (Color)v[4]);
        break;
    case SCENE_OP_MENU:
        if (machine->frame->menu != NULL) {
            machine->frame->menu->draw(machine->frame->menu->context, canvas, rect_from(v));
        }
        break;
    case SCENE_OP_REFLECT:
        if (machine->frame->cover.pixels != NULL) {
            image_draw_reflection(canvas, machine->frame->cover,
                                  (Quad){v[0], v[2], v[1], v[2], v[1], v[2]},
                                  (QuadLight){REFLECTION_LIGHT, REFLECTION_LIGHT}, v[3]);
        }
        break;
    case SCENE_OP_BACKDROP:
        backdrop_paint(canvas, machine->frame->theme, rect_from(v));
        break;
    default:
        draw_text_op(machine, op, canvas);
        break;
    }
}

ECHO_NP_FACES_CODE static int part_of(SceneOp op)
{
    switch (op) {
    case SCENE_OP_COVER:
    case SCENE_OP_ROUND_COVER:
        return DRAW_PART_COVERS;
    case SCENE_OP_REFLECT:
        return DRAW_PART_REFLECTIONS;
    case SCENE_OP_TEXT:
    case SCENE_OP_LINE:
    case SCENE_OP_PLAIN:
    case SCENE_OP_TIMES:
    case SCENE_OP_FORMAT:
    case SCENE_OP_CODEC:
        return DRAW_PART_TEXT;
    case SCENE_OP_MENU:
    case SCENE_OP_BACKDROP:
        return DRAW_PART_COUNT;
    default:
        return DRAW_PART_SHAPES;
    }
}

ECHO_NP_FACES_CODE static void timed_draw(Machine *machine, SceneOp op)
{
    uint32_t started_us = draw_profile_now();
    draw(machine, op);
    int part = part_of(op);
    if (part < DRAW_PART_COUNT) {
        draw_profile_since((DrawPart)part, started_us);
    }
}

ECHO_NP_FACES_CODE static void load_inputs(Machine *machine)
{
    const NowPlayingFrame *frame = machine->frame;
    int32_t *r = machine->registers;
    r[SCENE_INPUT_X] = frame->area.x;
    r[SCENE_INPUT_Y] = frame->area.y;
    r[SCENE_INPUT_WIDTH] = frame->area.width;
    r[SCENE_INPUT_HEIGHT] = frame->area.height;
    r[SCENE_INPUT_TICK] = wrapped(frame->tick);
    r[SCENE_INPUT_NOW] = wrapped(frame->now_ms);
    r[SCENE_INPUT_SHOWN] = wrapped(frame->shown_ms);
    r[SCENE_INPUT_DURATION] = wrapped(frame->status->duration_ms);
    r[SCENE_INPUT_VOLUME] = frame->status->volume;
    r[SCENE_INPUT_PLAYING] = frame->playing;
    r[SCENE_INPUT_PAUSED] = frame->status->state == PLAYER_PAUSED;
    r[SCENE_INPUT_SCRUBBING] = frame->scrubbing;
    r[SCENE_INPUT_VOLUME_SHOWN] = frame->volume_shown;
    r[SCENE_INPUT_BADGE] = (int32_t)face_badge_kind(frame);
    r[SCENE_INPUT_SHUFFLE] = (int32_t)frame->status->shuffle;
    r[SCENE_INPUT_REPEAT] = (int32_t)frame->status->repeat;
    r[SCENE_INPUT_TRACK] = wrapped(frame->status->place.number);
    r[SCENE_INPUT_HAS_COVER] = frame->cover.pixels != NULL;
    machine->feedback_text = face_feedback_text(frame, machine->feedback, sizeof machine->feedback);
    face_times_text(frame, &machine->times);
}

ECHO_NP_FACES_CODE static uint32_t jump_target(const Machine *machine, SceneOp op)
{
    return machine->raw[op == SCENE_OP_JUMP ? 0 : 1];
}

ECHO_NP_FACES_CODE void scene_draw(const NowPlayingFrame *frame, Canvas *canvas)
{
    const SceneCode *scene = &frame->scene;
    if (scene->work == NULL || scene->work_size < sizeof(Machine)) {
        return;
    }
    Machine *machine = scene->work;
    *machine = (Machine){.frame = frame};
    machine->canvases[0] = *canvas;
    load_inputs(machine);
    uint32_t at = 0;
    for (uint32_t step = 0; step < SCENE_MOST_STEPS && scene->code != NULL && at < scene->code_size;
         step++) {
        SceneOp op = (SceneOp)scene->code[at];
        if (op >= SCENE_OP_COUNT || op == SCENE_OP_END ||
            at + 1 + 2u * COUNTS[op] > scene->code_size) {
            return;
        }
        for (uint32_t index = 0; index < COUNTS[op]; index++) {
            const uint8_t *field = scene->code + at + 1 + 2 * index;
            machine->raw[index] = (uint16_t)(field[0] | field[1] << 8);
            bool raw = (RAW[op] >> index & 1u) != 0;
            machine->values[index] =
                raw ? machine->raw[index] : operand(machine, machine->raw[index]);
        }
        at += 1 + 2u * COUNTS[op];
        if (op == SCENE_OP_JUMP || op == SCENE_OP_JUMP_ZERO) {
            bool taken = op == SCENE_OP_JUMP || machine->values[0] == 0;
            at = taken ? jump_target(machine, op) : at;
        } else if (KINDS[op] == SCENE_KIND_DRAW) {
            timed_draw(machine, op);
        } else {
            store(machine, compute(machine, op));
        }
    }
}

#include "ui/list_view.h"

#include <stddef.h>

#include "gfx/text.h"
#include "ui/label.h"

enum {
    MIN_THUMB_HEIGHT = 6,
    SHINE_HEIGHT = 1,
    OUTLINE_SIDE_INSET = 6,
    OUTLINE_EDGE_INSET = 1,
    OUTLINE_THICKNESS = 1,
    NARROW_DETAIL_SHARE = 30,
    PERCENT = 100,
    SECTION_HEIGHT_SHARE = 2,
    SECTION_SHARE_PARTS_HIGH = 3,
    SECTION_WIDTH_SHARE = 4,
    SECTION_SHARE_PARTS_WIDE = 7
};

static const char CHEVRON[] = "\xE2\x80\xBA";
static const char SECTION_LABELS[][2] = {"#", "A", "B", "C", "D", "E", "F", "G", "H", "I",
                                         "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S",
                                         "T", "U", "V", "W", "X", "Y", "Z", "#"};

typedef struct Row {
    int index;
    Rect area;
    Rect text;
    bool selected;
} Row;

static int item_count(const ListView *view)
{
    return view->model->count(view->model->context);
}

static int max_of(int first, int second)
{
    return first > second ? first : second;
}

int list_view_rows(const ListView *view)
{
    int rows = view->visible_rows > 0 ? view->visible_rows : theme_visible_rows(view->theme);
    return max_of(1, rows);
}

static int top_showing_selection(const ListView *view)
{
    int rows = list_view_rows(view);
    if (view->selected < view->top) {
        return view->selected;
    }
    if (view->selected >= view->top + rows) {
        return view->selected - rows + 1;
    }
    return view->top;
}

int list_view_top(const ListView *view)
{
    return top_showing_selection(view);
}

static void keep_selection_visible(ListView *view)
{
    view->top = top_showing_selection(view);
}

static int wrapped_target(int target, int count, bool wrap)
{
    if (target < 0) {
        return wrap ? count - 1 : 0;
    }
    if (target >= count) {
        return wrap ? 0 : count - 1;
    }
    return target;
}

void list_view_init(ListView *view, const ListModel *model, const Theme *theme, uint32_t now_ms)
{
    *view = (ListView){
        .model = model,
        .theme = theme,
        .selection_changed_ms = now_ms,
        .section_row = -1,
    };
}

static int section_at(const ListView *view, int index)
{
    return view->model->section(view->model->context, index);
}

static bool section_showing(const ListView *view, uint32_t now_ms)
{
    return view->model->section != NULL && (int32_t)(now_ms - view->section_shown_until_ms) < 0;
}

static void note_section(ListView *view, uint32_t now_ms)
{
    if (!section_showing(view, now_ms)) {
        view->section_row = -1;
        return;
    }
    if (view->section_row == view->selected) {
        return;
    }
    view->section = section_at(view, view->selected);
    view->section_row = view->selected;
}

void list_view_move(ListView *view, int delta, bool wrap, uint32_t now_ms)
{
    int count = item_count(view);
    if (count == 0) {
        return;
    }
    int target = wrapped_target(view->selected + delta, count, wrap);
    if (target == view->selected) {
        return;
    }
    view->selected = target;
    view->selection_changed_ms = now_ms;
    keep_selection_visible(view);
    note_section(view, now_ms);
}

void list_view_start_scroll(ListView *view, int direction, uint32_t now_ms)
{
    view->scroll_started_ms = now_ms;
    list_view_move(view, direction, true, now_ms);
}

static int first_of_section_at_least(const ListView *view, int section, int low, int high)
{
    while (low < high) {
        int middle = low + (high - low) / 2;
        if (section_at(view, middle) >= section) {
            high = middle;
        } else {
            low = middle + 1;
        }
    }
    return low;
}

static int section_jump_target(const ListView *view, int direction)
{
    int count = item_count(view);
    int current = view->selected;
    int section = section_at(view, current);
    if (direction > 0) {
        return first_of_section_at_least(view, section + 1, current + 1, count);
    }
    int start = first_of_section_at_least(view, section, 0, current);
    if (start < current || current == 0) {
        return start;
    }
    return first_of_section_at_least(view, section_at(view, current - 1), 0, current - 1);
}

void list_view_keep_scrolling(ListView *view, int direction, uint32_t now_ms)
{
    uint32_t held_ms = now_ms - view->scroll_started_ms;
    if (held_ms < LIST_FAST_SCROLL_AFTER_MS) {
        list_view_move(view, direction, false, now_ms);
        return;
    }
    bool sectioned = view->model->section != NULL;
    if (sectioned) {
        view->section_shown_until_ms = now_ms + LIST_SECTION_SHOWN_MS;
    }
    if (held_ms < LIST_SECTION_JUMP_AFTER_MS) {
        list_view_move(view, direction * LIST_FAST_SCROLL_ROWS, false, now_ms);
    } else if (sectioned) {
        if (now_ms - view->section_jumped_ms >= LIST_SECTION_JUMP_EVERY_MS) {
            view->section_jumped_ms = now_ms;
            list_view_select(view, section_jump_target(view, direction), now_ms);
        }
    } else {
        list_view_move(view, direction * LIST_FASTEST_SCROLL_ROWS, false, now_ms);
    }
    note_section(view, now_ms);
}

int list_section_of_key(const char *key)
{
    unsigned char first = (unsigned char)key[0];
    if (first < 'a') {
        return LIST_SECTION_BEFORE_LETTERS;
    }
    if (first > 'z') {
        return LIST_SECTION_AFTER_LETTERS;
    }
    return LIST_SECTION_FIRST_LETTER + (first - 'a');
}

const char *list_section_label(int section)
{
    if (section < LIST_SECTION_BEFORE_LETTERS || section > LIST_SECTION_AFTER_LETTERS) {
        return SECTION_LABELS[LIST_SECTION_BEFORE_LETTERS];
    }
    return SECTION_LABELS[section];
}

void list_view_select(ListView *view, int index, uint32_t now_ms)
{
    int count = item_count(view);
    if (count == 0) {
        return;
    }
    int target = index < 0 ? 0 : index >= count ? count - 1 : index;
    view->selected = target;
    view->selection_changed_ms = now_ms;
    keep_selection_visible(view);
    note_section(view, now_ms);
}

void list_view_restart_marquee(ListView *view, uint32_t now_ms)
{
    view->selection_changed_ms = now_ms;
}

static TextStyle row_style(const ListView *view, bool selected, Color normal)
{
    Color color = selected ? theme_selected_text(view->theme) : normal;
    return (TextStyle){.font = view->theme->fonts.body, .color = color};
}

static Row draw_chevron(const ListView *view, Canvas *canvas, Row row)
{
    const ListModel *model = view->model;
    if (model->opens == NULL || !model->opens(model->context, row.index)) {
        return row;
    }
    Color color = row.selected ? theme_selected_text(view->theme) : view->theme->colors.chevron;
    TextStyle style = {.font = view->theme->fonts.title, .color = color};
    int width = text_width(style.font, CHEVRON);
    Rect area = {row.text.x + row.text.width - width, row.text.y, width, row.text.height};
    label_draw(canvas, style, (Label){.area = area, .text = CHEVRON});
    row.text.width -= width + view->theme->metrics.detail_gap;
    return row;
}

static Row draw_detail(const ListView *view, Canvas *canvas, Row row)
{
    const ListModel *model = view->model;
    const char *detail = model->detail != NULL ? model->detail(model->context, row.index) : NULL;
    if (detail == NULL) {
        return row;
    }
    TextStyle style = row_style(view, row.selected, view->theme->colors.text_secondary);
    bool narrow = theme_is_portrait(view->theme);
    if (narrow) {
        style.font = view->theme->fonts.small;
    }
    int width = text_width(style.font, detail);
    if (model->detail_max_width > 0 && width > model->detail_max_width) {
        width = model->detail_max_width;
    }
    if (narrow) {
        const char *label = model->label(model->context, row.index);
        int label_width = text_width(view->theme->fonts.body, label);
        int room = row.text.width - label_width - view->theme->metrics.detail_gap;
        int smallest = row.text.width * NARROW_DETAIL_SHARE / PERCENT;
        int widest = room > smallest ? room : smallest;
        width = width < widest ? width : widest;
    }
    Rect area = {row.text.x + row.text.width - width, row.text.y, width, row.text.height};
    label_draw(canvas, style, (Label){.area = area, .text = detail});
    row.text.width -= width + view->theme->metrics.detail_gap;
    return row;
}

static void draw_label(const ListView *view, Canvas *canvas, Row row, uint32_t now_ms)
{
    const char *text = view->model->label(view->model->context, row.index);
    TextStyle style = row_style(view, row.selected, view->theme->colors.text);
    Label label = {.area = row.text, .text = text};
    int width = text_width(style.font, text);
    if (!row.selected || width <= row.text.width) {
        label_draw(canvas, style, label);
        return;
    }
    MarqueeTiming timing = view->theme->marquee;
    int offset = marquee_offset(timing, width, row.text.width, now_ms - view->selection_changed_ms);
    label_draw_scrolling(canvas, style, label, (Marquee){.offset = offset, .gap = timing.gap_px});
}

static Rect inset_rect(Rect area, int across, int down)
{
    return (Rect){area.x + across, area.y + down, area.width - 2 * across, area.height - 2 * down};
}

static void draw_bar_selection(Canvas *canvas, Rect area, const ThemeColors *colors, bool flat)
{
    Color top = flat ? colors->selection_background : colors->selection_top;
    Rect shine = {area.x, area.y, area.width, SHINE_HEIGHT};
    Rect bar = {area.x, area.y + SHINE_HEIGHT, area.width, area.height - SHINE_HEIGHT};
    canvas_fill_rect(canvas, shine, flat ? top : colors->selection_shine);
    canvas_fill_gradient(canvas, bar, top, colors->selection_background);
}

static void draw_outline_selection(Canvas *canvas, Rect area, const ThemeColors *colors)
{
    Rect frame = inset_rect(area, OUTLINE_SIDE_INSET, OUTLINE_EDGE_INSET);
    canvas_fill_rect(canvas, frame, colors->selection_top);
    canvas_fill_rect(canvas, inset_rect(frame, OUTLINE_THICKNESS, OUTLINE_THICKNESS),
                     colors->background);
}

static void draw_selection(const ListView *view, Canvas *canvas, Rect area)
{
    const ThemeColors *colors = &view->theme->colors;
    if (view->theme->selection == THEME_SELECTION_OUTLINE) {
        draw_outline_selection(canvas, area, colors);
        return;
    }
    draw_bar_selection(canvas, area, colors, view->theme->selection == THEME_SELECTION_FLAT);
}

static void draw_row(const ListView *view, Canvas *canvas, Row row, uint32_t now_ms)
{
    if (row.selected) {
        draw_selection(view, canvas, row.area);
    }
    draw_label(view, canvas, draw_detail(view, canvas, draw_chevron(view, canvas, row)), now_ms);
}

static void draw_scrollbar(const ListView *view, Canvas *canvas, Rect area, int top)
{
    const Theme *theme = view->theme;
    int rows = list_view_rows(view);
    int hidden = item_count(view) - rows;
    int inset = theme->metrics.scrollbar_inset;
    Rect track = {area.x + area.width - inset - theme->metrics.scrollbar_width, area.y + inset,
                  theme->metrics.scrollbar_width, area.height - 2 * inset};
    int thumb_height = max_of(MIN_THUMB_HEIGHT, track.height * rows / item_count(view));
    int shown = top < hidden ? top : hidden;
    int thumb_top = track.y + (track.height - thumb_height) * shown / hidden;
    canvas_fill_rect(canvas, (Rect){track.x, thumb_top, track.width, thumb_height},
                     theme->colors.scrollbar_thumb);
}

static Row make_row(const ListView *view, Rect rows, int top, int position)
{
    const ThemeMetrics *metrics = &view->theme->metrics;
    int index = top + position;
    Rect area = {rows.x, rows.y + position * metrics->row_height, rows.width, metrics->row_height};
    Rect text = {area.x + metrics->padding, area.y, area.width - 2 * metrics->padding, area.height};
    return (Row){.index = index, .area = area, .text = text, .selected = index == view->selected};
}

static void draw_section(const ListView *view, Canvas *canvas, Rect area, uint32_t now_ms)
{
    if (!section_showing(view, now_ms) || item_count(view) == 0 ||
        view->section_row != view->selected) {
        return;
    }
    const Theme *theme = view->theme;
    int tall = area.height * SECTION_HEIGHT_SHARE / SECTION_SHARE_PARTS_HIGH;
    int wide = area.width * SECTION_WIDTH_SHARE / SECTION_SHARE_PARTS_WIDE;
    int side = tall < wide ? tall : wide;
    Rect box = {area.x + (area.width - side) / 2, area.y + (area.height - side) / 2, side, side};
    Rect inner = {box.x + 1, box.y + 1, side - 2, side - 2};
    canvas_fill_rect(canvas, box, theme->colors.divider);
    canvas_fill_rect(canvas, inner, theme->colors.header_background);
    TextStyle style = {.font = theme->fonts.title, .color = theme->colors.header_text};
    int scale = max_of(1, side / (font_line_height(style.font) + 4));
    const char *label = list_section_label(view->section);
    text_draw_glyph_scaled(canvas, style, inner, (uint32_t)(unsigned char)label[0], scale);
}

void list_view_draw(const ListView *view, Canvas *canvas, Rect area, uint32_t now_ms)
{
    canvas_fill_rect(canvas, area, view->theme->colors.background);
    Rect rows = area;
    int visible = list_view_rows(view);
    int top = top_showing_selection(view);
    if (item_count(view) > visible) {
        rows.width -= view->theme->metrics.scrollbar_width + view->theme->metrics.scrollbar_inset;
        draw_scrollbar(view, canvas, area, top);
    }
    for (int position = 0; position < visible; position++) {
        Row row = make_row(view, rows, top, position);
        if (row.index >= item_count(view)) {
            break;
        }
        if (!rect_is_empty(rect_intersection(row.area, canvas->clip))) {
            draw_row(view, canvas, row, now_ms);
        }
    }
    draw_section(view, canvas, area, now_ms);
}

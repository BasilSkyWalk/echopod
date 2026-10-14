#include "ui/list_view.h"

#include <stddef.h>

#include "ui/label.h"

enum {
    MIN_THUMB_HEIGHT = 6
};

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

static void keep_selection_visible(ListView *view)
{
    if (view->selected < view->top) {
        view->top = view->selected;
    } else if (view->selected >= view->top + view->visible_rows) {
        view->top = view->selected - view->visible_rows + 1;
    }
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
        .visible_rows = theme_visible_rows(theme),
        .selection_changed_ms = now_ms,
    };
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
}

void list_view_restart_marquee(ListView *view, uint32_t now_ms)
{
    view->selection_changed_ms = now_ms;
}

static TextStyle row_style(const ListView *view, bool selected, Color normal)
{
    Color color = selected ? view->theme->colors.selection_text : normal;
    return (TextStyle){.font = view->theme->fonts.body, .color = color};
}

static Row draw_detail(const ListView *view, Canvas *canvas, Row row)
{
    const ListModel *model = view->model;
    const char *detail = model->detail != NULL ? model->detail(model->context, row.index) : NULL;
    if (detail == NULL) {
        return row;
    }
    TextStyle style = row_style(view, row.selected, view->theme->colors.text_secondary);
    int width = text_width(style.font, detail);
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

static void draw_row(const ListView *view, Canvas *canvas, Row row, uint32_t now_ms)
{
    if (row.selected) {
        canvas_fill_rect(canvas, row.area, view->theme->colors.selection_background);
    }
    draw_label(view, canvas, draw_detail(view, canvas, row), now_ms);
}

static void draw_scrollbar(const ListView *view, Canvas *canvas, Rect area)
{
    const Theme *theme = view->theme;
    int hidden = item_count(view) - view->visible_rows;
    int thumb_height =
        max_of(MIN_THUMB_HEIGHT, area.height * view->visible_rows / item_count(view));
    int thumb_top = area.y + (area.height - thumb_height) * view->top / hidden;
    Rect track = {area.x + area.width - theme->metrics.scrollbar_width, area.y,
                  theme->metrics.scrollbar_width, area.height};
    canvas_fill_rect(canvas, track, theme->colors.scrollbar_track);
    canvas_fill_rect(canvas, (Rect){track.x, thumb_top, track.width, thumb_height},
                     theme->colors.scrollbar_thumb);
}

static Row make_row(const ListView *view, Rect rows, int position)
{
    const ThemeMetrics *metrics = &view->theme->metrics;
    int index = view->top + position;
    Rect area = {rows.x, rows.y + position * metrics->row_height, rows.width, metrics->row_height};
    Rect text = {area.x + metrics->padding, area.y, area.width - 2 * metrics->padding, area.height};
    return (Row){.index = index, .area = area, .text = text, .selected = index == view->selected};
}

void list_view_draw(const ListView *view, Canvas *canvas, Rect area, uint32_t now_ms)
{
    canvas_fill_rect(canvas, area, view->theme->colors.background);
    Rect rows = area;
    if (item_count(view) > view->visible_rows) {
        rows.width -= view->theme->metrics.scrollbar_width;
        draw_scrollbar(view, canvas, area);
    }
    for (int position = 0; position < view->visible_rows; position++) {
        Row row = make_row(view, rows, position);
        if (row.index >= item_count(view)) {
            break;
        }
        if (!rect_is_empty(rect_intersection(row.area, canvas->clip))) {
            draw_row(view, canvas, row, now_ms);
        }
    }
}

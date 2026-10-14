#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/canvas.h"
#include "ui/theme.h"

typedef struct ListModel {
    const void *context;
    int (*count)(const void *context);
    const char *(*label)(const void *context, int index);
    const char *(*detail)(const void *context, int index);
} ListModel;

typedef struct ListView {
    const ListModel *model;
    const Theme *theme;
    int visible_rows;
    int selected;
    int top;
    uint32_t selection_changed_ms;
} ListView;

void list_view_init(ListView *view, const ListModel *model, const Theme *theme, uint32_t now_ms);
void list_view_move(ListView *view, int delta, bool wrap, uint32_t now_ms);
void list_view_restart_marquee(ListView *view, uint32_t now_ms);
void list_view_select(ListView *view, int index, uint32_t now_ms);
void list_view_draw(const ListView *view, Canvas *canvas, Rect area, uint32_t now_ms);

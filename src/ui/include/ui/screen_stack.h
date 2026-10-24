#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ui/screen.h"

enum {
    SCREEN_STACK_CAPACITY = 8
};

typedef struct ScreenStack {
    Screen *screens[SCREEN_STACK_CAPACITY];
    int depth;
} ScreenStack;

void screen_stack_init(ScreenStack *stack, Screen *root, uint32_t now_ms);
Screen *screen_stack_top(const ScreenStack *stack);
int screen_stack_depth(const ScreenStack *stack);
bool screen_stack_apply(ScreenStack *stack, ScreenAction action, uint32_t now_ms);

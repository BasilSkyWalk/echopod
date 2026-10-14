#include "ui/screen_stack.h"

#include <stddef.h>

static void enter_top(ScreenStack *stack, uint32_t now_ms)
{
    Screen *top = screen_stack_top(stack);
    if (top->enter != NULL) {
        top->enter(top->context, now_ms);
    }
}

static bool push(ScreenStack *stack, Screen *target)
{
    if (target == NULL || stack->depth == SCREEN_STACK_CAPACITY) {
        return false;
    }
    stack->screens[stack->depth++] = target;
    return true;
}

static bool pop(ScreenStack *stack)
{
    if (stack->depth <= 1) {
        return false;
    }
    stack->depth--;
    return true;
}

static bool pop_to_root(ScreenStack *stack)
{
    if (stack->depth <= 1) {
        return false;
    }
    stack->depth = 1;
    return true;
}

static bool change(ScreenStack *stack, ScreenAction action)
{
    switch (action.type) {
    case SCREEN_ACTION_PUSH:
        return push(stack, action.target);
    case SCREEN_ACTION_POP:
        return pop(stack);
    case SCREEN_ACTION_POP_TO_ROOT:
        return pop_to_root(stack);
    case SCREEN_ACTION_NONE:
        return false;
    }
    return false;
}

void screen_stack_init(ScreenStack *stack, Screen *root, uint32_t now_ms)
{
    *stack = (ScreenStack){.screens = {root}, .depth = 1};
    enter_top(stack, now_ms);
}

Screen *screen_stack_top(const ScreenStack *stack)
{
    return stack->screens[stack->depth - 1];
}

int screen_stack_depth(const ScreenStack *stack)
{
    return stack->depth;
}

bool screen_stack_apply(ScreenStack *stack, ScreenAction action, uint32_t now_ms)
{
    if (!change(stack, action)) {
        return false;
    }
    enter_top(stack, now_ms);
    return true;
}

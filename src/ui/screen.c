#include "ui/screen.h"

#include <stddef.h>

ScreenAction screen_action_none(void)
{
    return (ScreenAction){.type = SCREEN_ACTION_NONE, .target = NULL};
}

ScreenAction screen_action_push(Screen *target)
{
    return (ScreenAction){.type = SCREEN_ACTION_PUSH, .target = target};
}

ScreenAction screen_action_pop(void)
{
    return (ScreenAction){.type = SCREEN_ACTION_POP, .target = NULL};
}

ScreenAction screen_action_pop_to_root(void)
{
    return (ScreenAction){.type = SCREEN_ACTION_POP_TO_ROOT, .target = NULL};
}

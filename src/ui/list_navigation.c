#include "ui/list_navigation.h"

#include <stddef.h>

#include "ui/navigation.h"

static ScreenAction run_on_current(const ListView *view,
                                   ScreenAction (*action)(void *context, int index), void *context)
{
    if (view->model->count(view->model->context) == 0 || action == NULL) {
        return screen_action_none();
    }
    return action(context, view->selected);
}

ScreenAction list_navigation_handle(ListView *view, KeyEvent event, uint32_t now_ms,
                                    ListSelection selection)
{
    Navigation navigation = navigation_from_key(event);
    switch (navigation.intent) {
    case NAVIGATION_PREVIOUS:
        list_view_move(view, -1, !navigation.repeated, now_ms);
        return screen_action_none();
    case NAVIGATION_NEXT:
        list_view_move(view, 1, !navigation.repeated, now_ms);
        return screen_action_none();
    case NAVIGATION_SELECT:
        return run_on_current(view, selection.select, selection.context);
    case NAVIGATION_OPTIONS:
        return run_on_current(view, selection.hold, selection.context);
    case NAVIGATION_BACK:
        return screen_action_pop();
    case NAVIGATION_HOME:
        return screen_action_pop_to_root();
    case NAVIGATION_NONE:
        return screen_action_none();
    }
    return screen_action_none();
}

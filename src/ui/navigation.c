#include "ui/navigation.h"

static Navigation navigation(NavigationIntent intent, bool repeated)
{
    return (Navigation){.intent = intent, .repeated = repeated};
}

static Navigation from_play(KeyGesture gesture)
{
    switch (gesture) {
    case KEY_GESTURE_PRESS:
        return navigation(NAVIGATION_SELECT, false);
    case KEY_GESTURE_LONG_PRESS:
        return navigation(NAVIGATION_OPTIONS, false);
    case KEY_GESTURE_REPEAT:
        return navigation(NAVIGATION_NONE, false);
    }
    return navigation(NAVIGATION_NONE, false);
}

static Navigation from_back(KeyGesture gesture)
{
    switch (gesture) {
    case KEY_GESTURE_PRESS:
        return navigation(NAVIGATION_BACK, false);
    case KEY_GESTURE_LONG_PRESS:
        return navigation(NAVIGATION_HOME, false);
    case KEY_GESTURE_REPEAT:
        return navigation(NAVIGATION_NONE, false);
    }
    return navigation(NAVIGATION_NONE, false);
}

Navigation navigation_from_key(KeyEvent event)
{
    bool repeated = event.gesture == KEY_GESTURE_REPEAT;
    switch (event.key) {
    case KEY_VOLUME_UP:
        return navigation(NAVIGATION_NEXT, repeated);
    case KEY_VOLUME_DOWN:
        return navigation(NAVIGATION_PREVIOUS, repeated);
    case KEY_PLAY:
        return from_play(event.gesture);
    case KEY_BACK:
        return from_back(event.gesture);
    case KEY_MENU:
    case KEY_POWER:
        return navigation(NAVIGATION_NONE, false);
    }
    return navigation(NAVIGATION_NONE, false);
}

ScreenAction navigation_exit_action(KeyEvent event)
{
    switch (navigation_from_key(event).intent) {
    case NAVIGATION_BACK:
        return screen_action_pop();
    case NAVIGATION_HOME:
        return screen_action_pop_to_root();
    case NAVIGATION_PREVIOUS:
    case NAVIGATION_NEXT:
    case NAVIGATION_SELECT:
    case NAVIGATION_OPTIONS:
    case NAVIGATION_NONE:
        return screen_action_none();
    }
    return screen_action_none();
}

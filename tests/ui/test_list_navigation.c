#include "check.h"
#include "ui/list_navigation.h"
#include "ui_test_theme.h"

enum {
    ITEM_COUNT = 5
};

static Screen target = {.title = "target"};
static int selected_index;

static int item_count(const void *context)
{
    (void)context;
    return ITEM_COUNT;
}

static const char *item_label(const void *context, int index)
{
    (void)context;
    (void)index;
    return "A";
}

static ScreenAction record_selection(void *context, int index)
{
    (void)context;
    selected_index = index;
    return screen_action_push(&target);
}

static const ListModel MODEL = {
    .context = NULL, .count = item_count, .label = item_label, .detail = NULL};

static ListView fresh_list(void)
{
    selected_index = -1;
    ListView view;
    list_view_init(&view, &MODEL, &TEST_THEME, 0);
    return view;
}

static int held_index;

static ScreenAction record_hold(void *context, int index)
{
    (void)context;
    held_index = index;
    return screen_action_none();
}

static ScreenAction send_with(ListView *view, KeyEvent event, ListSelection selection)
{
    return list_navigation_handle(view, event, 100, selection);
}

static ScreenAction send(ListView *view, Key key, KeyGesture gesture)
{
    ListSelection selection = {.select = record_selection, .hold = record_hold, .context = NULL};
    return send_with(view, (KeyEvent){key, gesture}, selection);
}

static void next_and_previous_move_the_selection(void)
{
    ListView view = fresh_list();
    CHECK_EQUAL(SCREEN_ACTION_NONE, send(&view, KEY_VOLUME_DOWN, KEY_GESTURE_PRESS).type);
    send(&view, KEY_VOLUME_DOWN, KEY_GESTURE_REPEAT);
    send(&view, KEY_VOLUME_UP, KEY_GESTURE_PRESS);
    CHECK_EQUAL(1, view.selected);
}

static void a_press_wraps_but_a_repeat_does_not(void)
{
    ListView view = fresh_list();
    send(&view, KEY_VOLUME_UP, KEY_GESTURE_REPEAT);
    CHECK_EQUAL(0, view.selected);
    send(&view, KEY_VOLUME_UP, KEY_GESTURE_PRESS);
    CHECK_EQUAL(ITEM_COUNT - 1, view.selected);
}

static void select_hands_the_selected_index_to_the_owner(void)
{
    ListView view = fresh_list();
    send(&view, KEY_VOLUME_DOWN, KEY_GESTURE_PRESS);
    send(&view, KEY_VOLUME_DOWN, KEY_GESTURE_PRESS);
    ScreenAction action = send(&view, KEY_PLAY, KEY_GESTURE_PRESS);
    CHECK_EQUAL(2, selected_index);
    CHECK(action.target == &target);
}

static void holding_play_hands_the_index_to_the_hold_action(void)
{
    ListView view = fresh_list();
    held_index = -1;
    send(&view, KEY_VOLUME_DOWN, KEY_GESTURE_PRESS);
    send(&view, KEY_PLAY, KEY_GESTURE_LONG_PRESS);
    CHECK_EQUAL(1, held_index);
    CHECK_EQUAL(-1, selected_index);
}

static void holding_play_without_a_hold_action_does_nothing(void)
{
    ListView view = fresh_list();
    ListSelection selection = {.select = record_selection, .hold = NULL, .context = NULL};
    KeyEvent hold = {KEY_PLAY, KEY_GESTURE_LONG_PRESS};
    CHECK_EQUAL(SCREEN_ACTION_NONE, send_with(&view, hold, selection).type);
    CHECK_EQUAL(-1, selected_index);
}

static void back_pops_and_a_long_back_goes_home(void)
{
    ListView view = fresh_list();
    CHECK_EQUAL(SCREEN_ACTION_POP, send(&view, KEY_BACK, KEY_GESTURE_PRESS).type);
    CHECK_EQUAL(SCREEN_ACTION_POP_TO_ROOT, send(&view, KEY_BACK, KEY_GESTURE_LONG_PRESS).type);
    CHECK_EQUAL(-1, selected_index);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(next_and_previous_move_the_selection),
        TEST_CASE(a_press_wraps_but_a_repeat_does_not),
        TEST_CASE(select_hands_the_selected_index_to_the_owner),
        TEST_CASE(holding_play_hands_the_index_to_the_hold_action),
        TEST_CASE(holding_play_without_a_hold_action_does_nothing),
        TEST_CASE(back_pops_and_a_long_back_goes_home),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

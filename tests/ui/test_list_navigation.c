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

static ScreenAction send(ListView *view, Key key, KeyGesture gesture)
{
    ListSelection selection = {.select = record_selection, .context = NULL};
    return list_navigation_handle(view, (KeyEvent){key, gesture}, 100, selection);
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
        TEST_CASE(back_pops_and_a_long_back_goes_home),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

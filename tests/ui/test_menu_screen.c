#include <stdbool.h>
#include <stddef.h>

#include "check.h"
#include "ui/menu_screen.h"
#include "ui_test_canvas.h"
#include "ui_test_theme.h"

static Screen first_target = {.title = "first"};
static Screen second_target = {.title = "second"};
static Screen last_target = {.title = "last"};
static bool toggled;

static ScreenAction toggle(void *context)
{
    bool *flag = context;
    *flag = !*flag;
    return screen_action_none();
}

static const char *toggle_value(const void *context)
{
    const bool *flag = context;
    return *flag ? "A" : "B";
}

static const MenuItem ITEMS[] = {
    {.label = "A", .select = menu_open_screen, .context = &first_target, .opens = true},
    {.label = "B", .select = menu_open_screen, .context = &second_target},
    {.label = "A"},
    {.label = "B", .select = toggle, .context = &toggled, .value = toggle_value},
    {.label = "A", .select = menu_open_screen, .context = &last_target, .opens = true},
};

static MenuScreen menu;

static Screen *open_menu(void)
{
    toggled = false;
    MenuConfig config = {.title = "Menu", .items = ITEMS, .item_count = 5, .theme = &TEST_THEME};
    Screen *screen = menu_screen_init(&menu, config);
    screen->enter(screen->context, 0);
    return screen;
}

static ScreenAction press(Screen *screen, Key key, KeyGesture gesture)
{
    return screen->handle_key(screen->context, (KeyEvent){key, gesture}, 100);
}

static void next(Screen *screen, int times)
{
    for (int step = 0; step < times; step++) {
        press(screen, KEY_VOLUME_UP, KEY_GESTURE_PRESS);
    }
}

static void the_menu_title_is_the_screen_title(void)
{
    Screen *screen = open_menu();
    CHECK(screen->title == menu.config.title);
}

static void selecting_the_first_item_opens_its_screen(void)
{
    Screen *screen = open_menu();
    ScreenAction action = press(screen, KEY_PLAY, KEY_GESTURE_PRESS);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, action.type);
    CHECK(action.target == &first_target);
}

static void moving_next_then_selecting_opens_the_second_screen(void)
{
    Screen *screen = open_menu();
    next(screen, 1);
    CHECK(press(screen, KEY_PLAY, KEY_GESTURE_PRESS).target == &second_target);
}

static void previous_on_the_first_item_wraps_to_the_last_on_a_press(void)
{
    Screen *screen = open_menu();
    press(screen, KEY_VOLUME_DOWN, KEY_GESTURE_PRESS);
    CHECK(press(screen, KEY_PLAY, KEY_GESTURE_PRESS).target == &last_target);
}

static void a_repeated_previous_does_not_wrap(void)
{
    Screen *screen = open_menu();
    press(screen, KEY_VOLUME_DOWN, KEY_GESTURE_REPEAT);
    CHECK(press(screen, KEY_PLAY, KEY_GESTURE_PRESS).target == &first_target);
}

static void back_pops_and_a_long_back_goes_home(void)
{
    Screen *screen = open_menu();
    CHECK_EQUAL(SCREEN_ACTION_POP, press(screen, KEY_BACK, KEY_GESTURE_PRESS).type);
    CHECK_EQUAL(SCREEN_ACTION_POP_TO_ROOT, press(screen, KEY_BACK, KEY_GESTURE_LONG_PRESS).type);
}

static void an_item_without_an_action_does_nothing(void)
{
    Screen *screen = open_menu();
    next(screen, 2);
    CHECK_EQUAL(SCREEN_ACTION_NONE, press(screen, KEY_PLAY, KEY_GESTURE_PRESS).type);
}

static void an_item_action_can_change_state_in_place(void)
{
    Screen *screen = open_menu();
    next(screen, 3);
    CHECK_EQUAL(SCREEN_ACTION_NONE, press(screen, KEY_PLAY, KEY_GESTURE_PRESS).type);
    CHECK(toggled);
}

static void the_header_and_the_list_are_drawn(void)
{
    Screen *screen = open_menu();
    Canvas canvas = ui_test_canvas();
    screen->update(screen->context, 100);
    screen->draw(screen->context, &canvas);
    Rect header = {0, 0, 1, TEST_HEADER_HEIGHT - 1};
    Rect first_row = {0, TEST_HEADER_HEIGHT, 1, TEST_ROW_HEIGHT};
    CHECK(ui_test_area_has_only(&canvas, header, TEST_THEME.colors->header_background));
    CHECK(ui_test_area_has_only(&canvas, first_row, TEST_THEME.colors->selection_background));
}

static void items_that_open_a_screen_show_a_chevron(void)
{
    Screen *screen = open_menu();
    Canvas canvas = ui_test_canvas();
    screen->update(screen->context, 100);
    screen->draw(screen->context, &canvas);
    Rect first_right = {TEST_SCREEN_WIDTH - 8, TEST_HEADER_HEIGHT, 5, TEST_ROW_HEIGHT};
    Rect second_right = {TEST_SCREEN_WIDTH - 8, TEST_HEADER_HEIGHT + TEST_ROW_HEIGHT, 5,
                         TEST_ROW_HEIGHT};
    CHECK(!ui_test_area_has_only(&canvas, first_right, TEST_THEME.colors->selection_background));
    CHECK(ui_test_area_has_only(&canvas, second_right, TEST_THEME.colors->background));
}

static bool second_shown;

static bool is_second_shown(const void *context)
{
    (void)context;
    return second_shown;
}

static const MenuItem SOMETIMES_ITEMS[] = {
    {.label = "A", .select = menu_open_screen, .context = &first_target},
    {.label = "B", .select = menu_open_screen, .context = &second_target, .shown = is_second_shown},
    {.label = "A", .select = menu_open_screen, .context = &last_target},
};

static Screen *open_sometimes_menu(bool shown)
{
    second_shown = shown;
    MenuConfig config = {
        .title = "Menu", .items = SOMETIMES_ITEMS, .item_count = 3, .theme = &TEST_THEME};
    Screen *screen = menu_screen_init(&menu, config);
    screen->enter(screen->context, 0);
    return screen;
}

static void a_hidden_item_is_skipped(void)
{
    Screen *screen = open_sometimes_menu(false);
    CHECK_EQUAL(2, menu.model.count(menu.model.context));
    next(screen, 1);
    CHECK(press(screen, KEY_PLAY, KEY_GESTURE_PRESS).target == &last_target);
}

static void a_scene_learns_which_item_each_place_holds(void)
{
    open_sometimes_menu(false);
    CHECK_EQUAL(0, menu_screen_scene_slot(&menu, 0));
    CHECK_EQUAL(2, menu_screen_scene_slot(&menu, 1));
    second_shown = true;
    CHECK_EQUAL(1, menu_screen_scene_slot(&menu, 1));
    CHECK_EQUAL(2, menu_screen_scene_slot(&menu, 2));
}

static void an_item_appears_once_it_is_shown(void)
{
    Screen *screen = open_sometimes_menu(false);
    second_shown = true;
    CHECK_EQUAL(3, menu.model.count(menu.model.context));
    next(screen, 1);
    CHECK(press(screen, KEY_PLAY, KEY_GESTURE_PRESS).target == &second_target);
}

static Rect panel_area_seen;
static int panel_updates;

static void record_panel_update(void *context, uint32_t now_ms)
{
    (void)context;
    (void)now_ms;
    panel_updates++;
}

static void paint_panel(void *context, Canvas *canvas, Rect area)
{
    (void)context;
    panel_area_seen = area;
    canvas_fill_rect(canvas, area, TEST_THEME.colors->danger);
}

static void a_side_panel_takes_the_right_of_the_content(void)
{
    enum {
        PANEL_WIDTH = 12
    };
    panel_updates = 0;
    MenuConfig config = {.title = "Menu",
                         .items = ITEMS,
                         .item_count = 5,
                         .theme = &TEST_THEME,
                         .panel = {.width = PANEL_WIDTH,
                                   .update = record_panel_update,
                                   .draw = paint_panel,
                                   .context = NULL}};
    Screen *screen = menu_screen_init(&menu, config);
    screen->enter(screen->context, 0);
    screen->update(screen->context, 100);
    Canvas canvas = ui_test_canvas();
    screen->draw(screen->context, &canvas);
    Rect content = theme_content_area(&TEST_THEME);
    CHECK_EQUAL(1, panel_updates);
    CHECK_EQUAL(TEST_SCREEN_WIDTH - PANEL_WIDTH, panel_area_seen.x);
    CHECK_EQUAL(content.y, panel_area_seen.y);
    CHECK_EQUAL(PANEL_WIDTH, panel_area_seen.width);
    CHECK_EQUAL(content.height, panel_area_seen.height);
    Rect first_row_left = {0, TEST_HEADER_HEIGHT, TEST_SCREEN_WIDTH - PANEL_WIDTH, 1};
    CHECK(!ui_test_area_has_color(&canvas, first_row_left, TEST_THEME.colors->danger));
}

static void a_theme_without_the_panel_gives_the_list_the_whole_content(void)
{
    Theme plain = TEST_THEME;
    plain.no_menu_panel = true;
    panel_updates = 0;
    panel_area_seen = (Rect){0};
    MenuConfig config = {
        .title = "Menu",
        .items = ITEMS,
        .item_count = 5,
        .theme = &plain,
        .panel = {
            .width = 12, .update = record_panel_update, .draw = paint_panel, .context = NULL}};
    Screen *screen = menu_screen_init(&menu, config);
    screen->enter(screen->context, 0);
    screen->update(screen->context, 100);
    Canvas canvas = ui_test_canvas();
    screen->draw(screen->context, &canvas);
    CHECK_EQUAL(0, panel_updates);
    CHECK_EQUAL(0, panel_area_seen.width);
    CHECK(!ui_test_area_has_color(&canvas, theme_content_area(&plain), TEST_THEME.colors->danger));
}

static int scene_updates;
static bool scene_wanted;
static Rect scene_list_area;

static bool update_scene(void *context, uint32_t now_ms)
{
    (void)context;
    (void)now_ms;
    scene_updates++;
    return scene_wanted;
}

static bool draw_scene(void *context, Canvas *canvas, MenuScreen *shown)
{
    (void)context;
    scene_list_area = (Rect){0, TEST_HEADER_HEIGHT, 40, 2 * TEST_THEME.metrics.row_height};
    menu_screen_scene_list(shown, canvas, scene_list_area);
    return true;
}

static Screen *scene_menu(MenuScene *scene)
{
    MenuConfig config = {
        .title = "Menu",
        .items = ITEMS,
        .item_count = 5,
        .theme = &TEST_THEME,
        .panel = {.width = 12, .update = record_panel_update, .draw = paint_panel, .context = NULL},
        .scene = scene};
    return menu_screen_init(&menu, config);
}

static void a_scene_draws_the_menu_in_its_own_place(void)
{
    MenuScene scene = {update_scene, draw_scene, NULL, false};
    scene_wanted = true;
    scene_updates = 0;
    panel_updates = 0;
    panel_area_seen = (Rect){0};
    Screen *screen = scene_menu(&scene);
    screen->enter(screen->context, 0);
    screen->update(screen->context, 100);
    Canvas canvas = ui_test_canvas();
    screen->draw(screen->context, &canvas);
    CHECK_EQUAL(1, scene_updates);
    CHECK_EQUAL(0, panel_updates);
    CHECK_EQUAL(0, panel_area_seen.width);
    CHECK_EQUAL(2, list_view_rows(&menu.list));
    next(screen, 2);
    CHECK_EQUAL(2, list_view_rows(&menu.list));
    CHECK_EQUAL(1, list_view_top(&menu.list));
}

static void without_its_scene_the_menu_draws_as_usual(void)
{
    MenuScene scene = {update_scene, draw_scene, NULL, false};
    scene_wanted = false;
    panel_updates = 0;
    Screen *screen = scene_menu(&scene);
    screen->enter(screen->context, 0);
    screen->update(screen->context, 100);
    Canvas canvas = ui_test_canvas();
    screen->draw(screen->context, &canvas);
    CHECK_EQUAL(1, panel_updates);
    CHECK_EQUAL(TEST_SCREEN_WIDTH - 12, panel_area_seen.x);
    CHECK(list_view_rows(&menu.list) != 2);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_menu_title_is_the_screen_title),
        TEST_CASE(items_that_open_a_screen_show_a_chevron),
        TEST_CASE(a_hidden_item_is_skipped),
        TEST_CASE(a_scene_learns_which_item_each_place_holds),
        TEST_CASE(a_side_panel_takes_the_right_of_the_content),
        TEST_CASE(a_theme_without_the_panel_gives_the_list_the_whole_content),
        TEST_CASE(a_scene_draws_the_menu_in_its_own_place),
        TEST_CASE(without_its_scene_the_menu_draws_as_usual),
        TEST_CASE(an_item_appears_once_it_is_shown),
        TEST_CASE(selecting_the_first_item_opens_its_screen),
        TEST_CASE(moving_next_then_selecting_opens_the_second_screen),
        TEST_CASE(previous_on_the_first_item_wraps_to_the_last_on_a_press),
        TEST_CASE(a_repeated_previous_does_not_wrap),
        TEST_CASE(back_pops_and_a_long_back_goes_home),
        TEST_CASE(an_item_without_an_action_does_nothing),
        TEST_CASE(an_item_action_can_change_state_in_place),
        TEST_CASE(the_header_and_the_list_are_drawn),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

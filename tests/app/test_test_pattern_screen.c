#include "check.h"
#include "hal/display.h"
#include "screens/test_pattern_screen.h"

enum {
    START_MS = 1000
};

static Color pixels[HAL_DISPLAY_WIDTH * HAL_DISPLAY_HEIGHT];
static TestPatternScreen pattern;

static Screen *open_pattern(void)
{
    Screen *screen = test_pattern_screen_init(&pattern);
    screen->enter(screen->context, START_MS);
    return screen;
}

static Canvas draw_at(Screen *screen, uint32_t now_ms)
{
    Canvas canvas = canvas_make(pixels, (Rect){0, 0, HAL_DISPLAY_WIDTH, HAL_DISPLAY_HEIGHT});
    screen->update(screen->context, now_ms);
    screen->draw(screen->context, &canvas);
    return canvas;
}

static Color indicator_color(const Canvas *canvas, Key key)
{
    Rect indicator = test_pattern_key_indicator(key);
    return canvas_pixel_at(canvas, indicator.x + indicator.width / 2,
                           indicator.y + indicator.height / 2);
}

static ScreenAction send(Screen *screen, Key key, KeyGesture gesture, uint32_t now_ms)
{
    return screen->handle_key(screen->context, (KeyEvent){key, gesture}, now_ms);
}

static void the_first_color_bar_is_at_the_top_left(void)
{
    Screen *screen = open_pattern();
    Canvas canvas = draw_at(screen, START_MS);
    CHECK_EQUAL(test_pattern_first_bar_color(), canvas_pixel_at(&canvas, 0, 0));
}

static void indicators_start_idle(void)
{
    Screen *screen = open_pattern();
    Canvas canvas = draw_at(screen, START_MS);
    CHECK_EQUAL(test_pattern_idle_color(), indicator_color(&canvas, KEY_PLAY));
    CHECK_EQUAL(test_pattern_idle_color(), indicator_color(&canvas, KEY_POWER));
}

static void each_gesture_flashes_its_own_color(void)
{
    Screen *screen = open_pattern();
    send(screen, KEY_PLAY, KEY_GESTURE_PRESS, START_MS);
    send(screen, KEY_MENU, KEY_GESTURE_LONG_PRESS, START_MS);
    send(screen, KEY_VOLUME_UP, KEY_GESTURE_REPEAT, START_MS);
    Canvas canvas = draw_at(screen, START_MS + 10);
    CHECK_EQUAL(test_pattern_gesture_color(KEY_GESTURE_PRESS), indicator_color(&canvas, KEY_PLAY));
    CHECK_EQUAL(test_pattern_gesture_color(KEY_GESTURE_LONG_PRESS),
                indicator_color(&canvas, KEY_MENU));
    CHECK_EQUAL(test_pattern_gesture_color(KEY_GESTURE_REPEAT),
                indicator_color(&canvas, KEY_VOLUME_UP));
    CHECK_EQUAL(test_pattern_idle_color(), indicator_color(&canvas, KEY_BACK));
}

static void a_flash_fades_back_to_idle(void)
{
    Screen *screen = open_pattern();
    send(screen, KEY_BACK, KEY_GESTURE_PRESS, START_MS);
    Canvas canvas = draw_at(screen, START_MS + TEST_PATTERN_FLASH_MS);
    CHECK_EQUAL(test_pattern_idle_color(), indicator_color(&canvas, KEY_BACK));
}

static void only_a_long_back_leaves_the_screen(void)
{
    Screen *screen = open_pattern();
    CHECK_EQUAL(SCREEN_ACTION_NONE, send(screen, KEY_BACK, KEY_GESTURE_PRESS, START_MS).type);
    CHECK_EQUAL(SCREEN_ACTION_POP, send(screen, KEY_BACK, KEY_GESTURE_LONG_PRESS, START_MS).type);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_first_color_bar_is_at_the_top_left), TEST_CASE(indicators_start_idle),
        TEST_CASE(each_gesture_flashes_its_own_color),     TEST_CASE(a_flash_fades_back_to_idle),
        TEST_CASE(only_a_long_back_leaves_the_screen),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

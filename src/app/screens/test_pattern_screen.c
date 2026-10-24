#include "test_pattern_screen.h"

#include <stddef.h>

#include "core/deadline.h"
#include "hal/display.h"

enum {
    BAR_COUNT = 8,
    BAR_HEIGHT = 110,
    BAR_WIDTH = HAL_DISPLAY_WIDTH / BAR_COUNT,
    MOVER_SIZE = 20,
    MOVER_TOP = 116,
    MOVER_TRAVEL = HAL_DISPLAY_WIDTH - MOVER_SIZE,
    MOVER_SPEED_PX_PER_S = 150,
    MILLISECONDS_PER_SECOND = 1000,
    INDICATOR_LEFT = 8,
    INDICATOR_TOP = 144,
    INDICATOR_WIDTH = 44,
    INDICATOR_HEIGHT = 20,
    INDICATOR_GAP = 8
};

typedef struct Rgb {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} Rgb;

static const Rgb BAR_COLORS[BAR_COUNT] = {
    {255, 255, 255}, {255, 255, 0}, {0, 255, 255}, {0, 255, 0},
    {255, 0, 255},   {255, 0, 0},   {0, 0, 255},   {0, 0, 0},
};

static Color color_of(Rgb rgb)
{
    return color_from_rgb(rgb.red, rgb.green, rgb.blue);
}

Color test_pattern_first_bar_color(void)
{
    return color_of(BAR_COLORS[0]);
}

Color test_pattern_idle_color(void)
{
    return color_from_rgb(80, 80, 80);
}

Color test_pattern_gesture_color(KeyGesture gesture)
{
    switch (gesture) {
    case KEY_GESTURE_PRESS:
        return color_from_rgb(0, 200, 0);
    case KEY_GESTURE_LONG_PRESS:
        return color_from_rgb(255, 140, 0);
    case KEY_GESTURE_REPEAT:
        return color_from_rgb(0, 120, 255);
    }
    return test_pattern_idle_color();
}

Rect test_pattern_key_indicator(Key key)
{
    int left = INDICATOR_LEFT + (int)key * (INDICATOR_WIDTH + INDICATOR_GAP);
    return (Rect){left, INDICATOR_TOP, INDICATOR_WIDTH, INDICATOR_HEIGHT};
}

static int bounce_offset(uint32_t elapsed_ms)
{
    uint64_t distance = (uint64_t)elapsed_ms * MOVER_SPEED_PX_PER_S / MILLISECONDS_PER_SECOND;
    uint32_t period = 2 * MOVER_TRAVEL;
    uint32_t phase = (uint32_t)(distance % period);
    return (int)(phase <= MOVER_TRAVEL ? phase : period - phase);
}

static void draw_color_bars(Canvas *canvas)
{
    for (int bar = 0; bar < BAR_COUNT; bar++) {
        Rect area = {bar * BAR_WIDTH, 0, BAR_WIDTH, BAR_HEIGHT};
        canvas_fill_rect(canvas, area, color_of(BAR_COLORS[bar]));
    }
}

static void draw_mover(const TestPatternScreen *screen, Canvas *canvas)
{
    int left = bounce_offset(screen->now_ms - screen->started_at_ms);
    Rect area = {left, MOVER_TOP, MOVER_SIZE, MOVER_SIZE};
    canvas_fill_rect(canvas, area, color_from_rgb(255, 255, 255));
}

static Color indicator_color(const KeyFlash *flash)
{
    return flash->active ? test_pattern_gesture_color(flash->gesture) : test_pattern_idle_color();
}

static void draw_indicators(const TestPatternScreen *screen, Canvas *canvas)
{
    for (int index = 0; index < KEY_COUNT; index++) {
        Key key = (Key)index;
        canvas_fill_rect(canvas, test_pattern_key_indicator(key),
                         indicator_color(&screen->flashes[key]));
    }
}

static void draw(void *context, Canvas *canvas)
{
    const TestPatternScreen *screen = context;
    draw_color_bars(canvas);
    draw_mover(screen, canvas);
    draw_indicators(screen, canvas);
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    TestPatternScreen *screen = context;
    screen->flashes[event.key] = (KeyFlash){
        .gesture = event.gesture,
        .until_ms = now_ms + TEST_PATTERN_FLASH_MS,
        .active = true,
    };
    bool leaving = event.key == KEY_BACK && event.gesture == KEY_GESTURE_LONG_PRESS;
    return leaving ? screen_action_pop() : screen_action_none();
}

static void update(void *context, uint32_t now_ms)
{
    TestPatternScreen *screen = context;
    screen->now_ms = now_ms;
    for (int index = 0; index < KEY_COUNT; index++) {
        KeyFlash *flash = &screen->flashes[index];
        if (flash->active && deadline_reached(now_ms, flash->until_ms)) {
            flash->active = false;
        }
    }
}

static void enter(void *context, uint32_t now_ms)
{
    TestPatternScreen *pattern = context;
    pattern->started_at_ms = now_ms;
    pattern->now_ms = now_ms;
    for (int index = 0; index < KEY_COUNT; index++) {
        pattern->flashes[index].active = false;
    }
}

Screen *test_pattern_screen_init(TestPatternScreen *pattern)
{
    *pattern = (TestPatternScreen){0};
    pattern->screen = (Screen){
        .title = "Key test",
        .context = pattern,
        .full_screen = true,
        .stays_on_wake = true,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &pattern->screen;
}

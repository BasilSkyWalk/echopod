#include "brick_screen.h"

#include "core/i18n.h"
#include "core/string_builder.h"
#include "ui/header.h"
#include "ui/label.h"

enum {
    BRICK_COLUMNS = 8,
    BRICK_ROWS = 6,
    BRICK_LEFT = 18,
    BRICK_TOP = 36,
    BRICK_WIDTH = 34,
    BRICK_HEIGHT = 12,
    BRICK_GAP = 3,
    PADDLE_Y = 147,
    PADDLE_WIDTH = 42,
    BALL_SIZE = 6,
    STEP_MS = 18
};

static void reset(BrickScreen *brick, uint32_t now_ms)
{
    for (int row = 0; row < BRICK_ROWS; row++) {
        brick->bricks[row] = (uint16_t)((1u << BRICK_COLUMNS) - 1u);
    }
    brick->ball_x = 157;
    brick->ball_y = 127;
    brick->ball_dx = 2;
    brick->ball_dy = -2;
    brick->paddle_x = 139;
    brick->score = 0;
    brick->launched = false;
    brick->game_over = false;
    brick->last_step_ms = now_ms;
}

static void enter(void *context, uint32_t now_ms)
{
    reset(context, now_ms);
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    BrickScreen *brick = context;
    if (event.key == KEY_BACK && event.gesture == KEY_GESTURE_PRESS) {
        return screen_action_pop();
    }
    if (event.key == KEY_PLAY && event.gesture == KEY_GESTURE_PRESS) {
        if (brick->game_over) {
            reset(brick, now_ms);
        } else {
            brick->launched = true;
        }
    } else if (event.key == KEY_VOLUME_UP) {
        brick->paddle_x += event.gesture == KEY_GESTURE_REPEAT ? 8 : 12;
    } else if (event.key == KEY_VOLUME_DOWN) {
        brick->paddle_x -= event.gesture == KEY_GESTURE_REPEAT ? 8 : 12;
    }
    if (brick->paddle_x < 0) {
        brick->paddle_x = 0;
    }
    if (brick->paddle_x > 278) {
        brick->paddle_x = 278;
    }
    return screen_action_none();
}

static void step_ball(BrickScreen *brick)
{
    if (!brick->launched || brick->game_over) {
        return;
    }
    brick->ball_x += brick->ball_dx;
    brick->ball_y += brick->ball_dy;
    if (brick->ball_x <= 0 || brick->ball_x + BALL_SIZE >= 320) {
        brick->ball_dx = -brick->ball_dx;
    }
    if (brick->ball_y <= 29) {
        brick->ball_dy = -brick->ball_dy;
    }
    if (brick->ball_dy > 0 && brick->ball_y + BALL_SIZE >= PADDLE_Y &&
        brick->ball_y <= PADDLE_Y + 5 && brick->ball_x + BALL_SIZE >= brick->paddle_x &&
        brick->ball_x <= brick->paddle_x + PADDLE_WIDTH) {
        brick->ball_dy = -2;
        int hit = brick->ball_x + BALL_SIZE / 2 - (brick->paddle_x + PADDLE_WIDTH / 2);
        brick->ball_dx = hit < -10 ? -3 : hit > 10 ? 3 : hit < 0 ? -2 : 2;
    }
    for (int row = 0; row < BRICK_ROWS; row++) {
        for (int column = 0; column < BRICK_COLUMNS; column++) {
            uint16_t bit = (uint16_t)(1u << column);
            if ((brick->bricks[row] & bit) == 0) {
                continue;
            }
            int x = BRICK_LEFT + column * (BRICK_WIDTH + BRICK_GAP);
            int y = BRICK_TOP + row * (BRICK_HEIGHT + BRICK_GAP);
            if (brick->ball_x + BALL_SIZE > x && brick->ball_x < x + BRICK_WIDTH &&
                brick->ball_y + BALL_SIZE > y && brick->ball_y < y + BRICK_HEIGHT) {
                brick->bricks[row] = (uint16_t)(brick->bricks[row] & (uint16_t)~bit);
                brick->ball_dy = -brick->ball_dy;
                brick->score++;
                if (brick->score == BRICK_COLUMNS * BRICK_ROWS) {
                    brick->game_over = true;
                }
                return;
            }
        }
    }
    if (brick->ball_y > 170) {
        brick->game_over = true;
    }
}

static void update(void *context, uint32_t now_ms)
{
    BrickScreen *brick = context;
    while (now_ms - brick->last_step_ms >= STEP_MS) {
        brick->last_step_ms += STEP_MS;
        step_ball(brick);
    }
}

static void draw(void *context, Canvas *canvas)
{
    const BrickScreen *brick = context;
    const Theme *theme = brick->theme;
    Rect content = theme_content_area(theme);
    canvas_fill_rect(canvas, content, theme->colors.background);
    header_draw(canvas, theme, brick->screen.title);
    for (int row = 0; row < BRICK_ROWS; row++) {
        for (int column = 0; column < BRICK_COLUMNS; column++) {
            if ((brick->bricks[row] & (uint16_t)(1u << column)) == 0) {
                continue;
            }
            Rect block = {BRICK_LEFT + column * (BRICK_WIDTH + BRICK_GAP),
                          BRICK_TOP + row * (BRICK_HEIGHT + BRICK_GAP), BRICK_WIDTH, BRICK_HEIGHT};
            canvas_fill_rect(canvas, block, theme->colors.success);
        }
    }
    canvas_fill_rect(canvas, (Rect){brick->paddle_x, PADDLE_Y, PADDLE_WIDTH, 5},
                     theme->colors.text);
    canvas_fill_rect(canvas, (Rect){brick->ball_x, brick->ball_y, BALL_SIZE, BALL_SIZE},
                     theme->colors.text);
    char score[12];
    StringBuilder builder = string_builder_make(score, sizeof score);
    string_builder_append(&builder, "Score ");
    string_builder_append_unsigned(&builder, brick->score);
    if (!brick->launched && !brick->game_over) {
        string_builder_append(&builder, " · ");
        string_builder_append(&builder, i18n_text(STR_MENU_BRICK_START));
    } else if (brick->game_over) {
        string_builder_append(&builder, " · ");
        string_builder_append(&builder, i18n_text(STR_MENU_BRICK_AGAIN));
    }
    label_draw(canvas, (TextStyle){.font = theme->fonts.small,
                                   .color = theme->colors.text_secondary},
               (Label){.area = {8, 151, 304, 16}, .text = score});
}

Screen *brick_screen_init(BrickScreen *brick, const Theme *theme)
{
    *brick = (BrickScreen){.theme = theme};
    reset(brick, 0);
    brick->screen = (Screen){.title = i18n_text(STR_MENU_BRICK),
                             .full_screen = true,
                             .paints_every_pixel = true,
                             .context = brick,
                             .enter = enter,
                             .handle_key = handle_key,
                             .update = update,
                             .draw = draw};
    return &brick->screen;
}

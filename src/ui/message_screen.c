#include "ui/message_screen.h"

#include <stddef.h>

#include "ui/header.h"
#include "ui/label.h"
#include "ui/navigation.h"

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    (void)context;
    (void)now_ms;
    return navigation_exit_action(event);
}

static void update(void *context, uint32_t now_ms)
{
    (void)context;
    (void)now_ms;
}

static void draw(void *context, Canvas *canvas)
{
    const MessageScreen *message = context;
    const Theme *theme = message->config.theme;
    Rect content = theme_content_area(theme);
    TextStyle style = {.font = theme->fonts.body, .color = theme->colors.text_secondary};
    canvas_fill_rect(canvas, content, theme->colors.background);
    header_draw(canvas, theme, message->config.title);
    Rect area = label_centered_area(style.font, content, message->config.message);
    label_draw(canvas, style, (Label){.area = area, .text = message->config.message});
}

Screen *message_screen_init(MessageScreen *message, MessageConfig config)
{
    *message = (MessageScreen){.config = config};
    message->screen = (Screen){
        .title = config.title,
        .context = message,
        .enter = NULL,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &message->screen;
}

#include "ui/message_screen.h"

#include <stddef.h>

#include "ui/header.h"
#include "ui/label.h"
#include "ui/navigation.h"

enum {
    MESSAGE_LINES = 4,
    LINE_CAPACITY = 128,
    LINE_GAP = 2
};

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

static size_t word_end(const char *text, size_t from)
{
    while (text[from] != '\0' && text[from] != ' ') {
        from++;
    }
    return from;
}

static void copy_span(char *line, const char *text, size_t count)
{
    size_t length = count < LINE_CAPACITY - 1 ? count : LINE_CAPACITY - 1;
    for (size_t index = 0; index < length; index++) {
        line[index] = text[index];
    }
    line[length] = '\0';
}

static size_t next_line(const Font *font, const char *text, int width, bool last, char *line)
{
    size_t fitted = word_end(text, 0);
    while (text[fitted] == ' ') {
        size_t end = word_end(text, fitted + 1);
        copy_span(line, text, end);
        if (!last && text_width(font, line) > width) {
            break;
        }
        fitted = end;
    }
    copy_span(line, text, fitted);
    while (text[fitted] == ' ') {
        fitted++;
    }
    return fitted;
}

static void draw(void *context, Canvas *canvas)
{
    const MessageScreen *message = context;
    const Theme *theme = message->config.theme;
    Rect content = theme_content_area(theme);
    TextStyle style = {.font = theme->fonts.body, .color = theme->colors.text_secondary};
    canvas_fill_rect(canvas, content, theme->colors.background);
    header_draw(canvas, theme, message->config.title);
    int padding = theme->metrics.padding;
    int width = content.width - 2 * padding;
    char lines[MESSAGE_LINES][LINE_CAPACITY];
    int count = 0;
    const char *rest = message->config.message;
    bool fits = text_width(style.font, rest) <= width;
    while (*rest != '\0' && count < MESSAGE_LINES) {
        rest +=
            next_line(style.font, rest, width, fits || count == MESSAGE_LINES - 1, lines[count]);
        count++;
    }
    int line_height = font_line_height(style.font) + LINE_GAP;
    int top = content.y + (content.height - count * line_height) / 2;
    for (int index = 0; index < count; index++) {
        Rect row = {content.x + padding, top + index * line_height, width, line_height};
        Rect area = label_centered_area(style.font, row, lines[index]);
        label_draw(canvas, style, (Label){.area = area, .text = lines[index]});
    }
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

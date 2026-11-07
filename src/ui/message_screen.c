#include "ui/message_screen.h"

#include <stddef.h>

#include "ui/header.h"
#include "ui/label.h"
#include "ui/navigation.h"

enum {
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

static size_t line_length(const char *line)
{
    size_t length = 0;
    while (line[length] != '\0') {
        length++;
    }
    return length;
}

static void lay_out(MessageLayout *layout, const char *message, const Font *font, int width)
{
    if (layout->font == font && layout->width == width) {
        return;
    }
    *layout = (MessageLayout){.font = font, .width = width};
    char line[LINE_CAPACITY];
    const char *rest = message;
    bool fits = text_width(font, rest) <= width;
    while (*rest != '\0' && layout->count < MESSAGE_SCREEN_LINES) {
        bool last = fits || layout->count == MESSAGE_SCREEN_LINES - 1;
        size_t taken = next_line(font, rest, width, last, line);
        layout->start[layout->count] = (uint16_t)(rest - message);
        layout->length[layout->count] = (uint16_t)line_length(line);
        layout->count++;
        rest += taken;
    }
}

static void draw(void *context, Canvas *canvas)
{
    MessageScreen *message = context;
    const Theme *theme = message->config.theme;
    Rect content = theme_content_area(theme);
    TextStyle style = {.font = theme->fonts.body, .color = theme->colors.text_secondary};
    canvas_fill_rect(canvas, content, theme->colors.background);
    header_draw(canvas, theme, message->config.title);
    int padding = theme->metrics.padding;
    int width = content.width - 2 * padding;
    MessageLayout *layout = &message->layout;
    lay_out(layout, message->config.message, style.font, width);
    int line_height = font_line_height(style.font) + LINE_GAP;
    int top = content.y + (content.height - layout->count * line_height) / 2;
    for (int index = 0; index < layout->count; index++) {
        Rect row = {content.x + padding, top + index * line_height, width, line_height};
        if (rect_is_empty(rect_intersection(row, canvas->clip))) {
            continue;
        }
        char line[LINE_CAPACITY];
        copy_span(line, message->config.message + layout->start[index], layout->length[index]);
        Rect area = label_centered_area(style.font, row, line);
        label_draw(canvas, style, (Label){.area = area, .text = line});
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

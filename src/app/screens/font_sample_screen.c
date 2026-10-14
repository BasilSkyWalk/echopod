#include "font_sample_screen.h"

#include <stdbool.h>
#include <stddef.h>

#include "ui/header.h"
#include "ui/label.h"
#include "ui/navigation.h"

enum {
    LINE_COUNT = 4,
    LINE_HEIGHT = 28,
    TOP_MARGIN = 6
};

typedef enum SampleFont {
    SAMPLE_TITLE,
    SAMPLE_BODY,
    SAMPLE_SMALL
} SampleFont;

typedef struct SampleLine {
    SampleFont font;
    bool secondary;
    const char *text;
} SampleLine;

static const SampleLine LINES[LINE_COUNT] = {
    {SAMPLE_TITLE, false, "Inter SemiBold 16 · Björk"},
    {SAMPLE_BODY, false, "The quick brown fox jumps over"},
    {SAMPLE_BODY, true, "Sigur Rós — Ágætis byrjun"},
    {SAMPLE_SMALL, false, "Inter 12 · 0123456789 ÀÉÎÕÜ ñ ß … – € ™"},
};

static const Font *font_of(const Theme *theme, SampleFont font)
{
    switch (font) {
    case SAMPLE_TITLE:
        return theme->fonts.title;
    case SAMPLE_BODY:
        return theme->fonts.body;
    case SAMPLE_SMALL:
        return theme->fonts.small;
    }
    return theme->fonts.body;
}

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

static void draw_line(Canvas *canvas, const Theme *theme, Rect content, int index)
{
    const SampleLine *line = &LINES[index];
    Color color = line->secondary ? theme->colors.text_secondary : theme->colors.text;
    TextStyle style = {.font = font_of(theme, line->font), .color = color};
    Rect area = {content.x + theme->metrics.padding, content.y + TOP_MARGIN + index * LINE_HEIGHT,
                 content.width - 2 * theme->metrics.padding, LINE_HEIGHT};
    label_draw(canvas, style, (Label){.area = area, .text = line->text});
}

static void draw(void *context, Canvas *canvas)
{
    const FontSampleScreen *sample = context;
    const Theme *theme = sample->theme;
    Rect content = theme_content_area(theme);
    canvas_fill_rect(canvas, content, theme->colors.background);
    header_draw(canvas, theme, sample->screen.title);
    for (int index = 0; index < LINE_COUNT; index++) {
        draw_line(canvas, theme, content, index);
    }
}

Screen *font_sample_screen_init(FontSampleScreen *sample, const Theme *theme)
{
    *sample = (FontSampleScreen){.theme = theme};
    sample->screen = (Screen){
        .title = "Fonts",
        .stays_when_idle = true,
        .context = sample,
        .enter = NULL,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &sample->screen;
}

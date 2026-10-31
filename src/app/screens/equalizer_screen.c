#include "equalizer_screen.h"

#include <stddef.h>

#include "core/string_builder.h"
#include "hal/audio.h"
#include "hal/clock.h"
#include "ui/header.h"
#include "ui/label.h"
#include "ui/list_navigation.h"
#include "ui/navigation.h"

enum {
    INFO_TOP = 28,
    INFO_HEIGHT = 20,
    COLUMN_TOP = 52,
    COLUMN_HEIGHT = 82,
    ZERO_Y = 93,
    PIXELS_PER_DB = 3,
    COLUMN_WIDTH = 44,
    COLUMN_LEFT = 6,
    TRACK_WIDTH = 2,
    BAR_WIDTH = 8,
    KNOB_WIDTH = 18,
    KNOB_HEIGHT = 4,
    LABEL_TOP = 136,
    HINT_TOP = 152,
    LINE_HEIGHT = 16
};

static const char *const PRESET_NAMES[EQUALIZER_PRESET_COUNT] = {
    "Off", "Retro", "Bass", "Heavy", "Pop", "Jazz", "Unique", "Custom"};
static const char *const BAND_LABELS[EQUALIZER_CUSTOM_BANDS] = {"63",   "160",  "400", "1k",
                                                                "2.5k", "6.3k", "16k"};
static const char *const BAND_NAMES[EQUALIZER_CUSTOM_BANDS] = {
    "63 Hz", "160 Hz", "400 Hz", "1 kHz", "2.5 kHz", "6.3 kHz", "16 kHz"};
static const char CHECK_MARK[] = "\xE2\x9C\x93";
static const char PICK_HINT[] = "+/-: choose band   Play: adjust   Hold Play: 0 dB";
static const char EDIT_HINT[] = "+/-: level   Play: done   Hold Play: 0 dB";
static const char BYPASS_HINT[] = "EQ is off for tracks above 96 kHz";

const char *equalizer_screen_preset_name(EqualizerPreset preset)
{
    return (unsigned)preset < EQUALIZER_PRESET_COUNT ? PRESET_NAMES[preset] : "";
}

static EqualizerChoice *choice_of(const EqualizerConfig *config)
{
    return &config->store->choice;
}

static bool above_equalizer_rate(void)
{
    return hal_audio_status().format.sample_rate > EQUALIZER_MAX_RATE;
}

static ScreenAction leave(const EqualizerConfig *config, ScreenAction action)
{
    bool leaving = action.type == SCREEN_ACTION_POP || action.type == SCREEN_ACTION_POP_TO_ROOT;
    if (leaving && !equalizer_store_save(config->store)) {
        toast_show(config->toast, "Could not save the equalizer", hal_clock_milliseconds());
    }
    return action;
}

static void use_preset(const EqualizerConfig *config, EqualizerPreset preset)
{
    EqualizerChoice *choice = choice_of(config);
    if (choice->preset != preset) {
        choice->preset = preset;
        equalizer_store_apply(config->store);
    }
}

static int preset_count(const void *context)
{
    (void)context;
    return EQUALIZER_PRESET_COUNT;
}

static const char *preset_label(const void *context, int index)
{
    (void)context;
    return PRESET_NAMES[index];
}

static const char *preset_mark(const void *context, int index)
{
    const EqualizerScreen *equalizer = context;
    return (int)choice_of(&equalizer->config)->preset == index ? CHECK_MARK : NULL;
}

static bool preset_opens(const void *context, int index)
{
    (void)context;
    return index == EQUALIZER_PRESET_CUSTOM;
}

static ScreenAction choose_preset(void *context, int index)
{
    EqualizerScreen *equalizer = context;
    if (index == EQUALIZER_PRESET_CUSTOM) {
        return screen_action_push(&equalizer->editor.screen);
    }
    return screen_action_pop();
}

static ScreenAction handle_picker_key(void *context, KeyEvent event, uint32_t now_ms)
{
    EqualizerScreen *equalizer = context;
    ListSelection selection = {.select = choose_preset, .hold = NULL, .context = equalizer};
    ScreenAction action = list_navigation_handle(&equalizer->list, event, now_ms, selection);
    use_preset(&equalizer->config, (EqualizerPreset)equalizer->list.selected);
    return leave(&equalizer->config, action);
}

static void enter_picker(void *context, uint32_t now_ms)
{
    EqualizerScreen *equalizer = context;
    equalizer->now_ms = now_ms;
    list_view_select(&equalizer->list, (int)choice_of(&equalizer->config)->preset, now_ms);
    if (above_equalizer_rate()) {
        toast_show(equalizer->config.toast, BYPASS_HINT, now_ms);
    }
}

static void update_picker(void *context, uint32_t now_ms)
{
    EqualizerScreen *equalizer = context;
    equalizer->now_ms = now_ms;
}

static void draw_picker(void *context, Canvas *canvas)
{
    const EqualizerScreen *equalizer = context;
    const Theme *theme = equalizer->config.theme;
    header_draw(canvas, theme, equalizer->screen.title);
    list_view_draw(&equalizer->list, canvas, theme_content_area(theme), equalizer->now_ms);
}

static int8_t *gains_of(const EqualizerEditor *editor)
{
    return choice_of(&editor->config)->custom_gains;
}

static void append_decibels(StringBuilder *text, int value)
{
    if (value != 0) {
        string_builder_append_char(text, value > 0 ? '+' : '-');
    }
    string_builder_append_unsigned(text, (uint32_t)(value < 0 ? -value : value));
    string_builder_append(text, " dB");
}

static int largest_boost(const int8_t *gains)
{
    int boost = 0;
    for (int band = 0; band < EQUALIZER_CUSTOM_BANDS; band++) {
        boost = gains[band] > boost ? gains[band] : boost;
    }
    return boost;
}

static void describe(EqualizerEditor *editor)
{
    const int8_t *gains = gains_of(editor);
    StringBuilder band = string_builder_make(editor->band_text, sizeof editor->band_text);
    string_builder_append(&band, BAND_NAMES[editor->band]);
    string_builder_append(&band, "   ");
    append_decibels(&band, gains[editor->band]);
    StringBuilder preamp = string_builder_make(editor->preamp_text, sizeof editor->preamp_text);
    int boost = largest_boost(gains);
    if (boost > 0) {
        string_builder_append(&preamp, "Preamp ");
        append_decibels(&preamp, -boost);
    }
}

static void change(EqualizerEditor *editor, int delta)
{
    int8_t *gain = &gains_of(editor)[editor->band];
    int next = *gain + delta;
    next = next < EQUALIZER_GAIN_MIN_DB   ? EQUALIZER_GAIN_MIN_DB
           : next > EQUALIZER_GAIN_MAX_DB ? EQUALIZER_GAIN_MAX_DB
                                          : next;
    if (next != *gain) {
        *gain = (int8_t)next;
        equalizer_store_apply(editor->config.store);
    }
    describe(editor);
}

static void move_band(EqualizerEditor *editor, int delta)
{
    int band = editor->band + delta;
    editor->band = band < 0                         ? 0
                   : band >= EQUALIZER_CUSTOM_BANDS ? EQUALIZER_CUSTOM_BANDS - 1
                                                    : band;
    describe(editor);
}

static void step(EqualizerEditor *editor, int delta)
{
    if (editor->editing) {
        change(editor, delta);
    } else {
        move_band(editor, delta);
    }
}

static ScreenAction handle_editor_key(void *context, KeyEvent event, uint32_t now_ms)
{
    (void)now_ms;
    EqualizerEditor *editor = context;
    NavigationIntent intent = navigation_from_key(event).intent;
    switch (intent) {
    case NAVIGATION_NEXT:
    case NAVIGATION_PREVIOUS:
        step(editor, intent == NAVIGATION_NEXT ? 1 : -1);
        return screen_action_none();
    case NAVIGATION_SELECT:
        editor->editing = !editor->editing;
        return screen_action_none();
    case NAVIGATION_OPTIONS:
        change(editor, -gains_of(editor)[editor->band]);
        return screen_action_none();
    case NAVIGATION_BACK:
        if (editor->editing) {
            editor->editing = false;
            return screen_action_none();
        }
        return leave(&editor->config, screen_action_pop());
    case NAVIGATION_HOME:
    case NAVIGATION_NONE:
        return leave(&editor->config, navigation_exit_action(event));
    }
    return screen_action_none();
}

static void enter_editor(void *context, uint32_t now_ms)
{
    (void)now_ms;
    EqualizerEditor *editor = context;
    use_preset(&editor->config, EQUALIZER_PRESET_CUSTOM);
    editor->editing = false;
    editor->bypassed = above_equalizer_rate();
    describe(editor);
}

static void draw_text(Canvas *canvas, TextStyle style, Rect area, const char *text)
{
    label_draw(canvas, style, (Label){.area = area, .text = text});
}

static void draw_centered(Canvas *canvas, TextStyle style, Rect area, const char *text)
{
    draw_text(canvas, style, label_centered_area(style.font, area, text), text);
}

static void draw_info(const EqualizerEditor *editor, Canvas *canvas)
{
    const Theme *theme = editor->config.theme;
    int padding = theme->metrics.padding;
    int width = theme->metrics.screen_width - 2 * padding;
    TextStyle band = {.font = theme->fonts.title,
                      .color = editor->editing ? theme->colors.selection_top : theme->colors.text};
    TextStyle preamp = {.font = theme->fonts.small, .color = theme->colors.text_secondary};
    draw_text(canvas, band, (Rect){padding, INFO_TOP, width, INFO_HEIGHT}, editor->band_text);
    int preamp_width = text_width(preamp.font, editor->preamp_text);
    draw_text(canvas, preamp,
              (Rect){padding + width - preamp_width, INFO_TOP, preamp_width, INFO_HEIGHT},
              editor->preamp_text);
}

static Rect bar_of(int center, int gain)
{
    int top = ZERO_Y - gain * PIXELS_PER_DB;
    int left = center - BAR_WIDTH / 2;
    return gain >= 0 ? (Rect){left, top, BAR_WIDTH, ZERO_Y - top}
                     : (Rect){left, ZERO_Y + 1, BAR_WIDTH, top - ZERO_Y};
}

static void draw_slider(const EqualizerEditor *editor, Canvas *canvas, int band)
{
    const ThemeColors *colors = &editor->config.theme->colors;
    bool selected = band == editor->band;
    int left = COLUMN_LEFT + band * COLUMN_WIDTH;
    int center = left + COLUMN_WIDTH / 2;
    int gain = gains_of(editor)[band];
    int knob_y = ZERO_Y - gain * PIXELS_PER_DB - KNOB_HEIGHT / 2;
    canvas_fill_rect(
        canvas, (Rect){center - TRACK_WIDTH / 2, COLUMN_TOP + 4, TRACK_WIDTH, COLUMN_HEIGHT - 8},
        colors->divider);
    bool grabbed = selected && editor->editing;
    canvas_fill_rect(canvas, bar_of(center, gain),
                     grabbed    ? colors->selection_shine
                     : selected ? colors->selection_background
                                : colors->text_tertiary);
    canvas_fill_rect(canvas, (Rect){center - KNOB_WIDTH / 2, knob_y, KNOB_WIDTH, KNOB_HEIGHT},
                     selected ? colors->selection_text : colors->text_secondary);
    TextStyle label = {.font = editor->config.theme->fonts.small,
                       .color = selected ? colors->text : colors->text_tertiary};
    draw_centered(canvas, label, (Rect){left, LABEL_TOP, COLUMN_WIDTH, LINE_HEIGHT},
                  BAND_LABELS[band]);
}

static void draw_editor(void *context, Canvas *canvas)
{
    const EqualizerEditor *editor = context;
    const Theme *theme = editor->config.theme;
    int width = theme->metrics.screen_width;
    header_draw(canvas, theme, editor->screen.title);
    draw_info(editor, canvas);
    canvas_fill_rect(
        canvas,
        (Rect){COLUMN_LEFT + editor->band * COLUMN_WIDTH, COLUMN_TOP, COLUMN_WIDTH, COLUMN_HEIGHT},
        editor->editing ? theme->colors.selection_background : theme->colors.header_top);
    canvas_fill_rect(canvas, (Rect){COLUMN_LEFT, ZERO_Y, width - 2 * COLUMN_LEFT, 1},
                     theme->colors.text_tertiary);
    for (int band = 0; band < EQUALIZER_CUSTOM_BANDS; band++) {
        draw_slider(editor, canvas, band);
    }
    TextStyle hint = {.font = theme->fonts.small, .color = theme->colors.text_tertiary};
    draw_centered(canvas, hint, (Rect){0, HINT_TOP, width, LINE_HEIGHT},
                  editor->bypassed  ? BYPASS_HINT
                  : editor->editing ? EDIT_HINT
                                    : PICK_HINT);
}

static void init_editor(EqualizerEditor *editor, EqualizerConfig config)
{
    *editor = (EqualizerEditor){.config = config};
    editor->screen = (Screen){
        .title = "Custom EQ",
        .context = editor,
        .enter = enter_editor,
        .handle_key = handle_editor_key,
        .draw = draw_editor,
    };
}

Screen *equalizer_screen_init(EqualizerScreen *equalizer, EqualizerConfig config)
{
    equalizer->config = config;
    equalizer->now_ms = 0;
    equalizer->model = (ListModel){.context = equalizer,
                                   .count = preset_count,
                                   .label = preset_label,
                                   .detail = preset_mark,
                                   .opens = preset_opens};
    list_view_init(&equalizer->list, &equalizer->model, config.theme, 0);
    equalizer->screen = (Screen){
        .title = "Equalizer",
        .context = equalizer,
        .enter = enter_picker,
        .handle_key = handle_picker_key,
        .update = update_picker,
        .draw = draw_picker,
    };
    init_editor(&equalizer->editor, config);
    return &equalizer->screen;
}

ScreenAction equalizer_screen_open(void *context)
{
    EqualizerScreen *equalizer = context;
    return screen_action_push(&equalizer->screen);
}

const char *equalizer_screen_summary(const void *context)
{
    const EqualizerScreen *equalizer = context;
    return equalizer_screen_preset_name(choice_of(&equalizer->config)->preset);
}

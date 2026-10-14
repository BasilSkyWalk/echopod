#include "sim_script.h"

#include <stddef.h>
#include <string.h>

typedef struct KeyName {
    const char *name;
    Key key;
} KeyName;

static const KeyName KEY_NAMES[] = {
    {"up", KEY_VOLUME_UP}, {"down", KEY_VOLUME_DOWN}, {"play", KEY_PLAY},
    {"menu", KEY_MENU},    {"back", KEY_BACK},        {"power", KEY_POWER},
};

static const char LONG_SUFFIX[] = ":long";

static bool has_long_suffix(const char *start, size_t length)
{
    size_t suffix_length = sizeof LONG_SUFFIX - 1;
    return length > suffix_length &&
           strncmp(start + length - suffix_length, LONG_SUFFIX, suffix_length) == 0;
}

static bool parse_step(const char *start, size_t length, SimScriptStep *step)
{
    bool long_press = has_long_suffix(start, length);
    size_t name_length = long_press ? length - (sizeof LONG_SUFFIX - 1) : length;
    for (size_t index = 0; index < sizeof KEY_NAMES / sizeof KEY_NAMES[0]; index++) {
        const char *name = KEY_NAMES[index].name;
        if (strlen(name) == name_length && strncmp(start, name, name_length) == 0) {
            *step = (SimScriptStep){.key = KEY_NAMES[index].key, .long_press = long_press};
            return true;
        }
    }
    return false;
}

bool sim_script_parse(const char *text, SimScript *script)
{
    *script = (SimScript){.count = 0};
    const char *start = text;
    for (;;) {
        const char *end = strchr(start, ',');
        size_t length = end != NULL ? (size_t)(end - start) : strlen(start);
        if (script->count == SIM_SCRIPT_MAX_STEPS ||
            !parse_step(start, length, &script->steps[script->count])) {
            return false;
        }
        script->count++;
        if (end == NULL) {
            return true;
        }
        start = end + 1;
    }
}

int sim_script_schedule(const SimScript *script, uint32_t first_frame, SimScriptEvent *events)
{
    int count = 0;
    uint32_t frame = first_frame;
    for (int index = 0; index < script->count; index++) {
        const SimScriptStep *step = &script->steps[index];
        uint32_t hold = step->long_press ? SIM_SCRIPT_HOLD_FRAMES : SIM_SCRIPT_TAP_FRAMES;
        events[count++] = (SimScriptEvent){.frame = frame, .key = step->key, .pressed = true};
        events[count++] =
            (SimScriptEvent){.frame = frame + hold, .key = step->key, .pressed = false};
        frame += hold + SIM_SCRIPT_GAP_FRAMES;
    }
    return count;
}

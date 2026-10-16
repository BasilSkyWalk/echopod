#include "check.h"
#include "sim_script.h"

static void key_names_map_to_device_keys(void)
{
    SimScript script;
    CHECK(sim_script_parse("up,down,play,menu,back,power", &script));
    CHECK_EQUAL(6, script.count);
    CHECK_EQUAL(KEY_VOLUME_DOWN, script.steps[0].key);
    CHECK_EQUAL(KEY_VOLUME_UP, script.steps[1].key);
    CHECK_EQUAL(KEY_PLAY, script.steps[2].key);
    CHECK_EQUAL(KEY_MENU, script.steps[3].key);
    CHECK_EQUAL(KEY_BACK, script.steps[4].key);
    CHECK_EQUAL(KEY_POWER, script.steps[5].key);
    CHECK(!script.steps[0].long_press);
}

static void a_long_suffix_holds_the_key(void)
{
    SimScript script;
    CHECK(sim_script_parse("back:long", &script));
    CHECK_EQUAL(1, script.count);
    CHECK(script.steps[0].long_press);
}

static void unknown_names_are_rejected(void)
{
    SimScript script;
    CHECK(!sim_script_parse("play,jump", &script));
    CHECK(!sim_script_parse("play:short", &script));
    CHECK(!sim_script_parse("", &script));
    CHECK(!sim_script_parse("play,,back", &script));
}

static void too_many_steps_are_rejected(void)
{
    char text[SIM_SCRIPT_MAX_STEPS * 3 + 8];
    int length = 0;
    for (int step = 0; step <= SIM_SCRIPT_MAX_STEPS; step++) {
        text[length++] = 'u';
        text[length++] = 'p';
        text[length++] = ',';
    }
    text[length - 1] = '\0';
    SimScript script;
    CHECK(!sim_script_parse(text, &script));
}

static void taps_press_and_release_then_leave_a_gap(void)
{
    SimScript script;
    sim_script_parse("play,down", &script);
    SimScriptEvent events[SIM_SCRIPT_MAX_EVENTS];
    CHECK_EQUAL(4, sim_script_schedule(&script, 10, events));
    CHECK_EQUAL(10, events[0].frame);
    CHECK(events[0].pressed);
    CHECK_EQUAL(10 + SIM_SCRIPT_TAP_FRAMES, events[1].frame);
    CHECK(!events[1].pressed);
    CHECK_EQUAL(10 + SIM_SCRIPT_TAP_FRAMES + SIM_SCRIPT_GAP_FRAMES, events[2].frame);
    CHECK_EQUAL(KEY_VOLUME_UP, events[2].key);
}

static void long_presses_hold_for_the_hold_time(void)
{
    SimScript script;
    sim_script_parse("back:long", &script);
    SimScriptEvent events[SIM_SCRIPT_MAX_EVENTS];
    CHECK_EQUAL(2, sim_script_schedule(&script, 0, events));
    CHECK_EQUAL(SIM_SCRIPT_HOLD_FRAMES, events[1].frame);
    CHECK(!events[1].pressed);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(key_names_map_to_device_keys),
        TEST_CASE(a_long_suffix_holds_the_key),
        TEST_CASE(unknown_names_are_rejected),
        TEST_CASE(too_many_steps_are_rejected),
        TEST_CASE(taps_press_and_release_then_leave_a_gap),
        TEST_CASE(long_presses_hold_for_the_hold_time),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

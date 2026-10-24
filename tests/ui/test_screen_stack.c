#include <stddef.h>

#include "check.h"
#include "ui/screen_stack.h"

enum {
    FAKE_COUNT = SCREEN_STACK_CAPACITY + 1
};

typedef struct FakeScreen {
    Screen screen;
    int entered;
    uint32_t entered_at_ms;
} FakeScreen;

static FakeScreen fakes[FAKE_COUNT];

static void record_enter(void *context, uint32_t now_ms)
{
    FakeScreen *fake = context;
    fake->entered++;
    fake->entered_at_ms = now_ms;
}

static ScreenStack stack_with_root(void)
{
    for (int index = 0; index < FAKE_COUNT; index++) {
        fakes[index] = (FakeScreen){
            .screen = {.title = "fake", .context = &fakes[index], .enter = record_enter},
        };
    }
    ScreenStack stack;
    screen_stack_init(&stack, &fakes[0].screen, 100);
    return stack;
}

static void the_root_is_entered_and_on_top(void)
{
    ScreenStack stack = stack_with_root();
    CHECK(screen_stack_top(&stack) == &fakes[0].screen);
    CHECK_EQUAL(1, screen_stack_depth(&stack));
    CHECK_EQUAL(1, fakes[0].entered);
    CHECK_EQUAL(100, fakes[0].entered_at_ms);
}

static void pushing_enters_the_new_screen(void)
{
    ScreenStack stack = stack_with_root();
    CHECK(screen_stack_apply(&stack, screen_action_push(&fakes[1].screen), 200));
    CHECK(screen_stack_top(&stack) == &fakes[1].screen);
    CHECK_EQUAL(2, screen_stack_depth(&stack));
    CHECK_EQUAL(1, fakes[1].entered);
    CHECK_EQUAL(200, fakes[1].entered_at_ms);
}

static void popping_reenters_the_revealed_screen(void)
{
    ScreenStack stack = stack_with_root();
    screen_stack_apply(&stack, screen_action_push(&fakes[1].screen), 200);
    CHECK(screen_stack_apply(&stack, screen_action_pop(), 300));
    CHECK(screen_stack_top(&stack) == &fakes[0].screen);
    CHECK_EQUAL(2, fakes[0].entered);
    CHECK_EQUAL(300, fakes[0].entered_at_ms);
}

static void the_root_cannot_be_popped(void)
{
    ScreenStack stack = stack_with_root();
    CHECK(!screen_stack_apply(&stack, screen_action_pop(), 200));
    CHECK(screen_stack_top(&stack) == &fakes[0].screen);
    CHECK_EQUAL(1, fakes[0].entered);
}

static void pop_to_root_leaves_only_the_root(void)
{
    ScreenStack stack = stack_with_root();
    screen_stack_apply(&stack, screen_action_push(&fakes[1].screen), 200);
    screen_stack_apply(&stack, screen_action_push(&fakes[2].screen), 300);
    CHECK(screen_stack_apply(&stack, screen_action_pop_to_root(), 400));
    CHECK_EQUAL(1, screen_stack_depth(&stack));
    CHECK(screen_stack_top(&stack) == &fakes[0].screen);
    CHECK_EQUAL(400, fakes[0].entered_at_ms);
}

static void pop_to_root_at_the_root_changes_nothing(void)
{
    ScreenStack stack = stack_with_root();
    CHECK(!screen_stack_apply(&stack, screen_action_pop_to_root(), 200));
    CHECK_EQUAL(1, fakes[0].entered);
}

static void a_full_stack_refuses_pushes(void)
{
    ScreenStack stack = stack_with_root();
    for (int index = 1; index < SCREEN_STACK_CAPACITY; index++) {
        CHECK(screen_stack_apply(&stack, screen_action_push(&fakes[index].screen), 200));
    }
    CHECK(!screen_stack_apply(&stack, screen_action_push(&fakes[FAKE_COUNT - 1].screen), 300));
    CHECK_EQUAL(SCREEN_STACK_CAPACITY, screen_stack_depth(&stack));
    CHECK_EQUAL(0, fakes[FAKE_COUNT - 1].entered);
}

static void pushing_nothing_is_refused(void)
{
    ScreenStack stack = stack_with_root();
    CHECK(!screen_stack_apply(&stack, screen_action_push(NULL), 200));
    CHECK_EQUAL(1, screen_stack_depth(&stack));
}

static void the_none_action_changes_nothing(void)
{
    ScreenStack stack = stack_with_root();
    CHECK(!screen_stack_apply(&stack, screen_action_none(), 200));
    CHECK_EQUAL(1, fakes[0].entered);
}

static void screens_without_an_enter_callback_can_be_shown(void)
{
    ScreenStack stack = stack_with_root();
    fakes[1].screen.enter = NULL;
    CHECK(screen_stack_apply(&stack, screen_action_push(&fakes[1].screen), 200));
    CHECK(screen_stack_top(&stack) == &fakes[1].screen);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_root_is_entered_and_on_top),
        TEST_CASE(pushing_enters_the_new_screen),
        TEST_CASE(popping_reenters_the_revealed_screen),
        TEST_CASE(the_root_cannot_be_popped),
        TEST_CASE(pop_to_root_leaves_only_the_root),
        TEST_CASE(pop_to_root_at_the_root_changes_nothing),
        TEST_CASE(a_full_stack_refuses_pushes),
        TEST_CASE(pushing_nothing_is_refused),
        TEST_CASE(the_none_action_changes_nothing),
        TEST_CASE(screens_without_an_enter_callback_can_be_shown),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

#include "check.h"
#include "ui_test_theme.h"

static void the_selected_text_is_the_selection_text_unless_the_selection_is_an_outline(void)
{
    Theme theme = TEST_THEME;
    theme.colors.selection_top = 0xF81F;
    theme.colors.selection_text = 0xFFE0;
    theme.selection = THEME_SELECTION_GRADIENT;
    CHECK_EQUAL(0xFFE0, theme_selected_text(&theme));
    theme.selection = THEME_SELECTION_FLAT;
    CHECK_EQUAL(0xFFE0, theme_selected_text(&theme));
    theme.selection = THEME_SELECTION_OUTLINE;
    CHECK_EQUAL(0xF81F, theme_selected_text(&theme));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_selected_text_is_the_selection_text_unless_the_selection_is_an_outline),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

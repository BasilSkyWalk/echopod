#include <string.h>

#include "check.h"
#include "np_faces/scene.h"
#include "np_faces_test.h"

#define SCENE_OP(id, name, kind, count, raw) OP_##id,
enum {
#include "np_faces/scene_ops.def"
    OP_COUNT
};
#undef SCENE_OP

enum {
    IMMEDIATE = 0x8000,
    FIRST_FREE = 40,
    MOST_BYTES = 256,
    RED = 0xF800
};

typedef struct Program {
    uint8_t bytes[MOST_BYTES];
    uint32_t size;
} Program;

static Color pixels[NP_TEST_PIXELS];

static void put(Program *program, uint16_t value)
{
    program->bytes[program->size++] = (uint8_t)value;
    program->bytes[program->size++] = (uint8_t)(value >> 8);
}

static void op(Program *program, int code)
{
    program->bytes[program->size++] = (uint8_t)code;
}

static uint16_t number(int value)
{
    return (uint16_t)(IMMEDIATE | ((unsigned)value & 0x7FFF));
}

static void rect(Program *program, int x, int y, uint16_t color)
{
    op(program, OP_RECT);
    put(program, number(x));
    put(program, number(y));
    put(program, number(4));
    put(program, number(4));
    put(program, color);
}

static void red(Program *program, uint16_t target)
{
    op(program, OP_CONST);
    put(program, target);
    put(program, RED);
    put(program, 0);
}

static NpTestScene run_themed(const Program *program, const SceneMenu *menu,
                              const ThemeIcons *icons)
{
    NpTestScene scene = np_test_scene(NP_FACE_STANDARD, NP_TEST_LAYOUTS[0], NP_TEST_PLAYING);
    static uint32_t work[SCENE_WORK_BYTES / sizeof(uint32_t)];
    scene.frame.scene = (SceneCode){program->bytes, program->size, NULL, 0, work, sizeof work};
    if (icons != NULL) {
        scene.theme.icons = icons;
    }
    np_test_clear(pixels);
    Canvas canvas = canvas_make(pixels, (Rect){0, 0, scene.layout.width, scene.layout.height});
    NowPlayingFrame frame = np_test_frame(&scene);
    frame.menu = menu;
    scene_draw(&frame, &canvas);
    return scene;
}

static NpTestScene run_with(const Program *program, const SceneMenu *menu)
{
    return run_themed(program, menu, NULL);
}

static NpTestScene run(const Program *program, const char *strings, uint32_t strings_size)
{
    NpTestScene scene = np_test_scene(NP_FACE_STANDARD, NP_TEST_LAYOUTS[0], NP_TEST_PLAYING);
    static uint32_t work[SCENE_WORK_BYTES / sizeof(uint32_t)];
    scene.frame.scene =
        (SceneCode){program->bytes, program->size, strings, strings_size, work, sizeof work};
    np_test_clear(pixels);
    Canvas canvas = canvas_make(pixels, (Rect){0, 0, scene.layout.width, scene.layout.height});
    NowPlayingFrame frame = np_test_frame(&scene);
    scene_draw(&frame, &canvas);
    return scene;
}

static const char *const LABELS[] = {"Music", "Settings", "Diagnostics"};
static Rect menu_area;
static int menu_draws;

static int label_count(const void *context)
{
    (void)context;
    return 3;
}

static const char *label_at(const void *context, int index)
{
    (void)context;
    return LABELS[index];
}

static void draw_menu(void *context, Canvas *canvas, Rect area)
{
    (void)context;
    (void)canvas;
    menu_area = area;
    menu_draws++;
}

static int slot_at(void *context, int position)
{
    (void)context;
    return position == 0 ? 0 : position + 2;
}

static const ListModel MODEL = {.count = label_count, .label = label_at};
static const SceneMenu MENU = {&MODEL, 2, draw_menu, slot_at, NULL};

static void value(Program *program, int code, uint16_t target)
{
    op(program, code);
    put(program, target);
}

static void plain_item(Program *program, int index, int y)
{
    op(program, OP_ITEM);
    put(program, FIRST_FREE + 3);
    put(program, number(index));
    op(program, OP_PLAIN);
    put(program, FIRST_FREE + 3);
    put(program, number(1));
    put(program, number(0));
    put(program, number(y));
    put(program, number(120));
    put(program, number(20));
    put(program, number(31));
}

static Color at(const NpTestScene *scene, int x, int y)
{
    return np_test_pixel(pixels, scene->layout, x, y);
}

static bool drawn_in(const NpTestScene *scene, Rect area)
{
    for (int y = area.y; y < area.y + area.height; y++) {
        for (int x = area.x; x < area.x + area.width; x++) {
            if (at(scene, x, y) != NP_TEST_SENTINEL) {
                return true;
            }
        }
    }
    return false;
}

static void a_scene_computes_and_draws(void)
{
    Program program = {.size = 0};
    red(&program, FIRST_FREE);
    op(&program, OP_ADD);
    put(&program, FIRST_FREE + 1);
    put(&program, number(10));
    put(&program, number(20));
    rect(&program, 0, 0, FIRST_FREE);
    op(&program, OP_ADD);
    put(&program, FIRST_FREE + 2);
    put(&program, FIRST_FREE + 1);
    put(&program, number(5));
    op(&program, OP_RECT);
    put(&program, FIRST_FREE + 2);
    put(&program, number(50));
    put(&program, number(2));
    put(&program, number(2));
    put(&program, FIRST_FREE);
    op(&program, OP_END);
    NpTestScene scene = run(&program, NULL, 0);
    CHECK_EQUAL(RED, at(&scene, 1, 1));
    CHECK_EQUAL(RED, at(&scene, 35, 50));
    CHECK_EQUAL(NP_TEST_SENTINEL, at(&scene, 34, 50));
}

static void an_empty_or_missing_scene_draws_nothing(void)
{
    Program program = {.size = 0};
    NpTestScene scene = run(&program, NULL, 0);
    CHECK_EQUAL(NP_TEST_SENTINEL, at(&scene, 1, 1));
    scene.frame.scene = (SceneCode){0};
    NowPlayingFrame frame = np_test_frame(&scene);
    Canvas canvas = canvas_make(pixels, (Rect){0, 0, scene.layout.width, scene.layout.height});
    scene_draw(&frame, &canvas);
    CHECK_EQUAL(NP_TEST_SENTINEL, at(&scene, 1, 1));
}

static void a_cut_short_or_unknown_op_stops_the_scene(void)
{
    Program program = {.size = 0};
    rect(&program, 0, 0, number(31));
    op(&program, OP_COUNT);
    rect(&program, 10, 10, number(31));
    NpTestScene scene = run(&program, NULL, 0);
    CHECK_EQUAL(31, at(&scene, 1, 1));
    CHECK_EQUAL(NP_TEST_SENTINEL, at(&scene, 11, 11));
    program.size = 0;
    rect(&program, 0, 0, number(31));
    rect(&program, 10, 10, number(31));
    program.size -= 3;
    scene = run(&program, NULL, 0);
    CHECK_EQUAL(31, at(&scene, 1, 1));
    CHECK_EQUAL(NP_TEST_SENTINEL, at(&scene, 11, 11));
}

static void an_endless_loop_stops_after_its_steps(void)
{
    Program program = {.size = 0};
    rect(&program, 0, 0, number(31));
    op(&program, OP_JUMP);
    put(&program, 0);
    NpTestScene scene = run(&program, NULL, 0);
    CHECK_EQUAL(31, at(&scene, 1, 1));
    program.size = 0;
    op(&program, OP_JUMP);
    put(&program, 60000);
    scene = run(&program, NULL, 0);
    CHECK_EQUAL(NP_TEST_SENTINEL, at(&scene, 1, 1));
}

static void inputs_and_registers_out_of_range_cannot_be_written(void)
{
    Program program = {.size = 0};
    op(&program, OP_SET);
    put(&program, 0);
    put(&program, number(100));
    op(&program, OP_SET);
    put(&program, 5000);
    put(&program, number(7));
    op(&program, OP_DIV);
    put(&program, FIRST_FREE);
    put(&program, number(9));
    put(&program, number(0));
    op(&program, OP_RECT);
    put(&program, 0);
    put(&program, FIRST_FREE);
    put(&program, number(4));
    put(&program, number(4));
    put(&program, number(31));
    NpTestScene scene = run(&program, NULL, 0);
    CHECK_EQUAL(31, at(&scene, 1, 1));
    CHECK_EQUAL(NP_TEST_SENTINEL, at(&scene, 101, 1));
}

static void text_past_the_string_table_is_left_out(void)
{
    static const char STRINGS[] = {'A', 'B', '\0', 'C'};
    Program program = {.size = 0};
    for (int index = 0; index < 3; index++) {
        op(&program, OP_TEXT);
        put(&program, number(SCENE_STRING_BASE + index));
        put(&program, number(1));
        put(&program, number(10));
        put(&program, number(30 + 20 * index));
        put(&program, number(100));
        put(&program, number(16));
        put(&program, number(0));
        put(&program, number(31));
    }
    NpTestScene scene = run(&program, STRINGS, sizeof STRINGS);
    bool first = false;
    bool rest = false;
    for (int y = 30; y < 90; y++) {
        for (int x = 10; x < 110; x++) {
            bool painted = at(&scene, x, y) == 31;
            first = first || (painted && y < 50);
            rest = rest || (painted && y >= 50);
        }
    }
    CHECK(first);
    CHECK(!rest);
}

static void clips_nest_and_extra_unclips_are_harmless(void)
{
    Program program = {.size = 0};
    for (int depth = 0; depth < 6; depth++) {
        op(&program, OP_CLIP);
        put(&program, number(0));
        put(&program, number(0));
        put(&program, number(20 - depth));
        put(&program, number(20));
    }
    rect(&program, 0, 0, number(31));
    op(&program, OP_RECT);
    put(&program, number(0));
    put(&program, number(0));
    put(&program, number(40));
    put(&program, number(2));
    put(&program, number(30));
    for (int depth = 0; depth < 8; depth++) {
        op(&program, OP_UNCLIP);
    }
    rect(&program, 30, 0, number(29));
    NpTestScene scene = run(&program, NULL, 0);
    CHECK_EQUAL(30, at(&scene, 16, 1));
    CHECK_EQUAL(NP_TEST_SENTINEL, at(&scene, 17, 1));
    CHECK_EQUAL(29, at(&scene, 31, 1));
}

static void a_home_scene_reads_the_menu(void)
{
    Program program = {.size = 0};
    value(&program, OP_ITEMS, FIRST_FREE);
    value(&program, OP_SELECTED, FIRST_FREE + 1);
    op(&program, OP_RECT);
    put(&program, FIRST_FREE);
    put(&program, FIRST_FREE + 1);
    put(&program, number(1));
    put(&program, number(1));
    put(&program, number(30));
    plain_item(&program, 1, 40);
    plain_item(&program, 3, 70);
    op(&program, OP_MENU);
    put(&program, number(5));
    put(&program, number(6));
    put(&program, number(70));
    put(&program, number(28));
    menu_draws = 0;
    NpTestScene scene = run_with(&program, &MENU);
    CHECK_EQUAL(30, at(&scene, 3, 2));
    CHECK(drawn_in(&scene, (Rect){0, 40, 120, 20}));
    CHECK(!drawn_in(&scene, (Rect){0, 70, 120, 20}));
    CHECK_EQUAL(1, menu_draws);
    CHECK_EQUAL(5, menu_area.x);
    CHECK_EQUAL(6, menu_area.y);
    CHECK_EQUAL(70, menu_area.width);
    CHECK_EQUAL(28, menu_area.height);
}

static void without_a_menu_the_menu_ops_draw_nothing(void)
{
    Program program = {.size = 0};
    value(&program, OP_ITEMS, FIRST_FREE);
    value(&program, OP_SELECTED, FIRST_FREE + 1);
    op(&program, OP_RECT);
    put(&program, FIRST_FREE);
    put(&program, FIRST_FREE + 1);
    put(&program, number(1));
    put(&program, number(1));
    put(&program, number(30));
    plain_item(&program, 0, 40);
    op(&program, OP_MENU);
    put(&program, number(0));
    put(&program, number(0));
    put(&program, number(10));
    put(&program, number(10));
    menu_draws = 0;
    NpTestScene scene = run_with(&program, NULL);
    CHECK_EQUAL(30, at(&scene, 0, 0));
    CHECK(!drawn_in(&scene, (Rect){0, 40, 120, 20}));
    CHECK_EQUAL(0, menu_draws);
}

static void item_icon(Program *program, int position, int x)
{
    op(program, OP_ITEM_ICON);
    put(program, number(position));
    put(program, number(x));
    put(program, number(10));
    put(program, number(2));
    put(program, number(2));
    put(program, FIRST_FREE);
}

static Program item_icons(void)
{
    Program program = {.size = 0};
    red(&program, FIRST_FREE);
    item_icon(&program, 0, 10);
    item_icon(&program, 1, 20);
    item_icon(&program, 3, 30);
    item_icon(&program, -1, 40);
    return program;
}

static const uint8_t SOLID_ICON[] = {0xFF, 0xFF};

static void a_tile_draws_the_icon_of_the_item_it_holds(void)
{
    static ThemeIcons icons;
    icons.masks[THEME_ICON_HOME_SETTINGS] = (IconMask){SOLID_ICON, 2, 2};
    Program program = item_icons();
    NpTestScene scene = run_themed(&program, &MENU, &icons);
    CHECK_EQUAL(RED, at(&scene, 20, 10));
    CHECK_EQUAL(RED, at(&scene, 21, 11));
    CHECK(!drawn_in(&scene, (Rect){10, 10, 2, 2}));
    CHECK(!drawn_in(&scene, (Rect){30, 10, 2, 2}));
    CHECK(!drawn_in(&scene, (Rect){40, 10, 2, 2}));
}

static void without_a_menu_or_an_icon_a_tile_draws_no_icon(void)
{
    static ThemeIcons icons;
    icons.masks[THEME_ICON_HOME_SETTINGS] = (IconMask){SOLID_ICON, 2, 2};
    Program program = item_icons();
    NpTestScene scene = run_themed(&program, NULL, &icons);
    CHECK(!drawn_in(&scene, (Rect){0, 10, 60, 2}));
    scene = run_with(&program, &MENU);
    CHECK(!drawn_in(&scene, (Rect){0, 10, 60, 2}));
}

static void a_backdrop_fills_its_area_with_the_theme(void)
{
    Program program = {.size = 0};
    op(&program, OP_BACKDROP);
    put(&program, number(0));
    put(&program, number(0));
    put(&program, number(8));
    put(&program, number(8));
    NpTestScene scene = run(&program, NULL, 0);
    CHECK(at(&scene, 7, 7) != NP_TEST_SENTINEL);
    CHECK_EQUAL(NP_TEST_SENTINEL, at(&scene, 8, 8));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_scene_computes_and_draws),
        TEST_CASE(an_empty_or_missing_scene_draws_nothing),
        TEST_CASE(a_cut_short_or_unknown_op_stops_the_scene),
        TEST_CASE(an_endless_loop_stops_after_its_steps),
        TEST_CASE(inputs_and_registers_out_of_range_cannot_be_written),
        TEST_CASE(text_past_the_string_table_is_left_out),
        TEST_CASE(clips_nest_and_extra_unclips_are_harmless),
        TEST_CASE(a_home_scene_reads_the_menu),
        TEST_CASE(without_a_menu_the_menu_ops_draw_nothing),
        TEST_CASE(a_backdrop_fills_its_area_with_the_theme),
        TEST_CASE(a_tile_draws_the_icon_of_the_item_it_holds),
        TEST_CASE(without_a_menu_or_an_icon_a_tile_draws_no_icon),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}

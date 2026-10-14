#include "app_screens.h"

#include <stddef.h>

static const char *const MUSIC_LABELS[MUSIC_ITEM_COUNT] = {
    "Cover Flow", "Artists",   "Album Artists", "Albums",    "Genres",
    "Folders",    "All Songs", "Playlists",     "Favorites",
};

static const char *year_value(const void *context)
{
    return context;
}

static const MenuItem LONG_NAME_ITEMS[] = {
    {"Sigur Rós — Ágætis byrjun", NULL, "1999", year_value},
    {"Godspeed You! Black Emperor — Lift Your Skinny Fists Like Antennas to Heaven", NULL, "2000",
     year_value},
    {"Björk — Homogenic", NULL, "1997", year_value},
    {"Sufjan Stevens — Illinois (Come On Feel the Illinoise)", NULL, "2005", year_value},
    {"Motörhead — Ace of Spades", NULL, "1980", year_value},
    {"Jóhann Jóhannsson — Orphée", NULL, "2016", year_value},
    {"Ólafur Arnalds — re:member", NULL, "2018", year_value},
    {"Röyksopp — Melody A.M.", NULL, "2001", year_value},
    {"Café Tacvba — Re", NULL, "1994", year_value},
    {"Beyoncé — Lemonade", NULL, "2016", year_value},
    {"Mötley Crüe — Dr. Feelgood", NULL, "1989", year_value},
    {"Daft Punk — Random Access Memories", NULL, "2013", year_value},
    {"Explosions in the Sky — The Earth Is Not a Cold Dead Place", NULL, "2003", year_value},
    {"Hania Rani — Esja", NULL, "2019", year_value},
    {"Nils Frahm — All Melody", NULL, "2018", year_value},
    {"The Avalanches — Since I Left You", NULL, "2000", year_value},
};

enum {
    LONG_NAME_COUNT = sizeof LONG_NAME_ITEMS / sizeof LONG_NAME_ITEMS[0]
};

static ScreenAction toggle_frame_time(void *context)
{
    AppOptions *options = context;
    options->show_frame_time = !options->show_frame_time;
    return screen_action_none();
}

static const char *frame_time_value(const void *context)
{
    const AppOptions *options = context;
    return options->show_frame_time ? "On" : "Off";
}

static MenuItem open_item(const char *label, Screen *target)
{
    return (MenuItem){.label = label, .select = menu_open_screen, .context = target, .value = NULL};
}

static Screen *build_menu(MenuScreen *menu, const char *title, const MenuItem *items, int count,
                          const Theme *theme)
{
    return menu_screen_init(menu, (MenuConfig){title, items, count, theme});
}

static void build_music(AppScreens *screens, const Theme *theme)
{
    for (int index = 0; index < MUSIC_ITEM_COUNT; index++) {
        screens->music_items[index] = open_item(MUSIC_LABELS[index], &screens->coming_soon.screen);
    }
    build_menu(&screens->music, "Music", screens->music_items, MUSIC_ITEM_COUNT, theme);
}

static void build_diagnostics(AppScreens *screens, AppOptions *options, const Theme *theme)
{
    Screen *long_names =
        build_menu(&screens->long_names, "Long names", LONG_NAME_ITEMS, LONG_NAME_COUNT, theme);
    screens->diagnostics_items[0] =
        open_item("Key test", test_pattern_screen_init(&screens->test_pattern));
    screens->diagnostics_items[1] =
        open_item("Fonts", font_sample_screen_init(&screens->font_sample, theme));
    screens->diagnostics_items[2] = open_item("Long names", long_names);
    screens->diagnostics_items[3] = (MenuItem){
        .label = "Frame time",
        .select = toggle_frame_time,
        .context = options,
        .value = frame_time_value,
    };
    build_menu(&screens->diagnostics, "Diagnostics", screens->diagnostics_items,
               DIAGNOSTICS_ITEM_COUNT, theme);
}

static void build_home(AppScreens *screens, const Theme *theme)
{
    screens->home_items[0] = open_item("Music", &screens->music.screen);
    screens->home_items[1] = open_item("Now Playing", &screens->coming_soon.screen);
    screens->home_items[2] = open_item("Settings", &screens->coming_soon.screen);
    screens->home_items[3] = open_item("Diagnostics", &screens->diagnostics.screen);
    build_menu(&screens->home, "Echo Mini", screens->home_items, HOME_ITEM_COUNT, theme);
}

Screen *app_screens_init(AppScreens *screens, AppOptions *options, const Theme *theme)
{
    MessageConfig coming_soon = {
        .title = "Coming soon", .message = "This part is not built yet", .theme = theme};
    message_screen_init(&screens->coming_soon, coming_soon);
    build_music(screens, theme);
    build_diagnostics(screens, options, theme);
    build_home(screens, theme);
    return &screens->home.screen;
}

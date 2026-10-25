add_library(echo_ui STATIC
    choice_screen.c
    cover_flow.c
    fade.c
    slide.c
    header.c
    icons.c
    label.c
    list_navigation.c
    list_view.c
    marquee.c
    menu_screen.c
    message_screen.c
    navigation.c
    screen.c
    screen_stack.c
    status_bar.c
    theme.c
    toast.c
)
target_include_directories(echo_ui PUBLIC include)
target_link_libraries(echo_ui
    PUBLIC echo_core echo_gfx
    PRIVATE echo_options echo_compact
)

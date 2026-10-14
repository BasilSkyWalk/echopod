add_library(echo_ui STATIC
    header.c
    label.c
    list_view.c
    marquee.c
    menu_screen.c
    message_screen.c
    navigation.c
    screen.c
    screen_stack.c
    theme.c
)
target_include_directories(echo_ui PUBLIC include)
target_link_libraries(echo_ui
    PUBLIC echo_core echo_gfx
    PRIVATE echo_options
)

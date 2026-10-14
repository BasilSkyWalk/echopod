add_library(echo_app STATIC
    app.c
    test_pattern_screen.c
)
target_include_directories(echo_app PUBLIC include)
target_link_libraries(echo_app
    PUBLIC echo_core echo_gfx echo_hal
    PRIVATE echo_options
)

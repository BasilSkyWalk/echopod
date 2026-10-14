add_library(echo_app STATIC
    app.c
    app_screens.c
    browse_screen.c
    default_theme.c
    frame_overlay.c
    library_store.c
    screens/font_sample_screen.c
    screens/test_pattern_screen.c
)
target_include_directories(echo_app PUBLIC include)
target_link_libraries(echo_app
    PUBLIC echo_core echo_gfx echo_ui echo_fonts echo_library echo_hal
    PRIVATE echo_options
)

add_library(echo_app STATIC
    app.c
    app_screens.c
    browse_screen.c
    cover_cache.c
    default_theme.c
    favorites_store.c
    frame_overlay.c
    library_store.c
    player.c
    settings_screens.c
    settings_store.c
    screens/cover_flow_screen.c
    screens/font_sample_screen.c
    screens/now_playing_screen.c
    screens/stopwatch_screen.c
    screens/test_pattern_screen.c
)
target_include_directories(echo_app PUBLIC include PRIVATE .)
target_link_libraries(echo_app
    PUBLIC echo_core echo_gfx echo_ui echo_fonts echo_library echo_playback echo_settings echo_hal
    PRIVATE echo_options
)

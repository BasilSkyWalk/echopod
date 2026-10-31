add_library(echo_settings STATIC settings.c rocker.c)
target_include_directories(echo_settings PUBLIC include)
target_link_libraries(echo_settings
    PUBLIC echo_core echo_playback
    PRIVATE echo_options echo_compact
)

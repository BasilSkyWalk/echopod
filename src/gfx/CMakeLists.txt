add_library(echo_gfx STATIC
    canvas.c
    color.c
    font.c
    rect.c
    strip_renderer.c
    text.c
)
target_include_directories(echo_gfx PUBLIC include)
target_link_libraries(echo_gfx
    PUBLIC echo_core
    PRIVATE echo_options
)

add_library(echo_gfx STATIC
    canvas.c
    color.c
    rect.c
    strip_renderer.c
)
target_include_directories(echo_gfx PUBLIC include)
target_link_libraries(echo_gfx PRIVATE echo_options)

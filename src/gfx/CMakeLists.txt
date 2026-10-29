add_library(echo_gfx STATIC
    canvas.c
    color.c
    font.c
    font_pack.c
    frame_diff.c
    image.c
    rect.c
    strip_renderer.c
    text.c
)
target_include_directories(echo_gfx PUBLIC include)
if(CMAKE_CROSSCOMPILING)
    set_source_files_properties(font_pack.c PROPERTIES COMPILE_OPTIONS -Os)
endif()
target_link_libraries(echo_gfx
    PUBLIC echo_core
    PRIVATE echo_options
)

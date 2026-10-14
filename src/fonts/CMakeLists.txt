add_library(echo_fonts STATIC fonts.c)
target_include_directories(echo_fonts PUBLIC include)
target_link_libraries(echo_fonts
    PUBLIC echo_gfx
    PRIVATE echo_options
)

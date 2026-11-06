add_library(echo_image STATIC
    cover_grid.c
    cover_image.c
    image_arena.c
    image_reader.c
    inflate.c
    jpeg.c
    jpeg_blocks.c
    jpeg_entropy.c
    png.c
)
target_include_directories(echo_image PUBLIC include)
target_link_libraries(echo_image
    PUBLIC echo_core
    PRIVATE echo_options
)

add_library(echo_library STATIC
    block_cache.c
    covers.c
    favorites.c
    library.c
    library_browse.c
)
target_include_directories(echo_library PUBLIC include)
target_link_libraries(echo_library
    PUBLIC echo_core
    PRIVATE echo_options
)

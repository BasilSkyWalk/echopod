add_library(echo_codec STATIC
    flac.c
    flac_bits.c
)
target_include_directories(echo_codec PUBLIC include)
target_link_libraries(echo_codec
    PUBLIC echo_core
    PRIVATE echo_options
)

add_library(echo_codec STATIC
    alac.c
    flac.c
    flac_bits.c
    mp4.c
    ogg.c
)
if(CMAKE_CROSSCOMPILING)
    set_source_files_properties(mp4.c ogg.c PROPERTIES COMPILE_OPTIONS -Os)
endif()
target_include_directories(echo_codec PUBLIC include)
target_link_libraries(echo_codec
    PUBLIC echo_core
    PRIVATE echo_options
)

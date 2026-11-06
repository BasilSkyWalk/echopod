add_library(echo_sound STATIC equalizer_file.c equalizer_presets.c equalizer_text.c)
target_include_directories(echo_sound PUBLIC include)
if(CMAKE_CROSSCOMPILING)
    target_compile_definitions(echo_sound PRIVATE ECHO_RESIDENT_TABLES)
endif()
target_link_libraries(echo_sound
    PUBLIC echo_core echo_dsp
    PRIVATE echo_options echo_compact echo_lto
)

add_library(echo_sound STATIC equalizer_file.c equalizer_presets.c equalizer_text.c)
target_include_directories(echo_sound PUBLIC include)
target_link_libraries(echo_sound
    PUBLIC echo_core echo_dsp
    PRIVATE echo_options echo_compact
)
if(CMAKE_CROSSCOMPILING)
    target_compile_options(echo_sound PRIVATE -Oz)
endif()

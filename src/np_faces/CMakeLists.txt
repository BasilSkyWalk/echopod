add_library(echo_np_faces STATIC
    np_faces.c
    face_cassette.c
    face_shapes.c
    face_text.c
    face_lcd.c
    face_minimal.c
    face_turntable.c
    face_vu.c
)
target_include_directories(echo_np_faces PUBLIC include PRIVATE .)
if(CMAKE_CROSSCOMPILING)
    target_compile_definitions(echo_np_faces PRIVATE ECHO_NP_FACES_SECTIONS)
endif()
target_link_libraries(echo_np_faces
    PUBLIC echo_core echo_gfx echo_ui echo_playback echo_settings
    PRIVATE echo_options echo_compact echo_lto
)

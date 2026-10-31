add_library(echo_sim_options STATIC sim_options.c sim_script.c)
target_include_directories(echo_sim_options PUBLIC .)
target_link_libraries(echo_sim_options PUBLIC echo_core PRIVATE echo_options)

find_package(SDL2 CONFIG REQUIRED)

add_library(echo_sim_codecs STATIC sim_codecs.c)
target_include_directories(echo_sim_codecs SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/third_party/dr_libs)
target_compile_options(echo_sim_codecs PRIVATE -w)

add_library(echo_sim_decoder STATIC sim_decoder.c sim_mp4.c sim_opus.c)
target_include_directories(echo_sim_decoder PUBLIC .)
target_link_libraries(echo_sim_decoder PUBLIC echo_hal echo_codec echo_faad2 echo_opus echo_sim_codecs PRIVATE echo_core echo_options)

string(REPLACE ";" "|" ECHO_GLYPH_FACE_PATHS "${ECHO_GLYPH_FACE_FILES}")
add_library(echo_sim_glyphs STATIC sim_glyphs.c)
target_include_directories(echo_sim_glyphs PUBLIC .)
target_compile_definitions(echo_sim_glyphs PRIVATE ECHO_GLYPH_FACE_PATHS="${ECHO_GLYPH_FACE_PATHS}")
target_link_libraries(echo_sim_glyphs PUBLIC echo_gfx PRIVATE echo_options)

add_library(echo_sim_storage STATIC sim_storage.c)
target_include_directories(echo_sim_storage PUBLIC .)
target_link_libraries(echo_sim_storage PUBLIC echo_hal PRIVATE echo_options)

add_executable(echo_sim
    main.c
    sim_audio.c
    sim_bluetooth.c
    sim_clock.c
    sim_display.c
    sim_input.c
    sim_power.c
    sim_system.c
)
target_compile_definitions(echo_sim PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(echo_sim PRIVATE echo_app echo_sim_glyphs echo_library_scan echo_sim_options echo_sim_decoder echo_sim_storage echo_options SDL2::SDL2)

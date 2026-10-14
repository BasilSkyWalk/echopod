add_library(echo_sim_options STATIC sim_options.c sim_script.c)
target_include_directories(echo_sim_options PUBLIC .)
target_link_libraries(echo_sim_options PUBLIC echo_core PRIVATE echo_options)

find_package(SDL2 CONFIG REQUIRED)

add_library(echo_sim_codecs STATIC sim_codecs.c)
target_include_directories(echo_sim_codecs SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/third_party/dr_libs)
target_compile_options(echo_sim_codecs PRIVATE -w)

add_executable(echo_sim
    main.c
    sim_audio.c
    sim_clock.c
    sim_decoder.c
    sim_display.c
    sim_input.c
    sim_power.c
    sim_storage.c
)
target_compile_definitions(echo_sim PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(echo_sim PRIVATE echo_app echo_sim_options echo_sim_codecs echo_options SDL2::SDL2)

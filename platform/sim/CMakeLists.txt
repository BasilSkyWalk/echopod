add_library(echo_sim_options STATIC sim_options.c)
target_include_directories(echo_sim_options PUBLIC .)
target_link_libraries(echo_sim_options PRIVATE echo_options)

find_package(SDL2 CONFIG REQUIRED)

add_executable(echo_sim
    main.c
    sim_clock.c
    sim_display.c
    sim_input.c
)
target_compile_definitions(echo_sim PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(echo_sim PRIVATE echo_app echo_sim_options echo_options SDL2::SDL2)

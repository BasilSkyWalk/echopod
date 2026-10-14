add_library(echo_core STATIC
    deadline.c
    frame_stats.c
    key_tracker.c
)
target_include_directories(echo_core PUBLIC include)
target_link_libraries(echo_core PRIVATE echo_options)

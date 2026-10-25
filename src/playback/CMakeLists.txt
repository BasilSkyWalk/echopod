add_library(echo_playback STATIC
    bookmarks.c
    play_queue.c
    resume_state.c
    spoken.c
    up_next.c
)
target_include_directories(echo_playback PUBLIC include)
target_link_libraries(echo_playback
    PUBLIC echo_core echo_library
    PRIVATE echo_options echo_compact
)

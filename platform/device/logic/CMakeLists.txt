add_library(echo_device_logic STATIC
    key_ladder.c
    key_scanner.c
    panel.c
    tick_clock.c
)
target_include_directories(echo_device_logic PUBLIC include)
target_link_libraries(echo_device_logic
    PUBLIC echo_core echo_hal
    PRIVATE echo_options
)

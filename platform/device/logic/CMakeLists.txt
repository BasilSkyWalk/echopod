add_library(echo_device_logic STATIC
    fat.c
    fat_layout.c
    key_ladder.c
    key_scanner.c
    panel.c
    sd_card.c
    tick_clock.c
)
target_include_directories(echo_device_logic PUBLIC include)
target_link_libraries(echo_device_logic
    PUBLIC echo_core echo_hal
    PRIVATE echo_options
)

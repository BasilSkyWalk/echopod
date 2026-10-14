add_library(echo_device_logic STATIC
    emmc_layout.c
    exfat.c
    fat.c
    fat_layout.c
    fat_names.c
    image_crc.c
    key_ladder.c
    key_scanner.c
    msc.c
    panel.c
    pll.c
    sd_card.c
    tick_clock.c
    updater.c
    usb_device.c
)
target_include_directories(echo_device_logic PUBLIC include)
target_link_libraries(echo_device_logic
    PUBLIC echo_core echo_hal
    PRIVATE echo_options
)

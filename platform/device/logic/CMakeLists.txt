add_library(echo_device_logic STATIC
    audio_engine.c
    audio_math.c
    battery_gauge.c
    core1_control.c
    core1_stream.c
    dac_sequence.c
    flac_source.c
    dma_loop.c
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
    start_record.c
    sd_card.c
    tick_clock.c
    updater.c
    usb_device.c
)
target_include_directories(echo_device_logic PUBLIC include)
target_link_libraries(echo_device_logic
    PUBLIC echo_core echo_hal echo_codec
    PRIVATE echo_options
)

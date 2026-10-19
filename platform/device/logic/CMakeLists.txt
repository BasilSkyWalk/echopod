set(ECHO_LIBMAD_DIR ${PROJECT_SOURCE_DIR}/third_party/libmad)
set(FPM_64BIT ON)
set(SIZEOF_INT 4)
configure_file(${ECHO_LIBMAD_DIR}/mad.h.in ${CMAKE_CURRENT_BINARY_DIR}/libmad/mad.h @ONLY)
add_library(echo_libmad STATIC
    ${ECHO_LIBMAD_DIR}/bit.c
    ${ECHO_LIBMAD_DIR}/fixed.c
    ${ECHO_LIBMAD_DIR}/frame.c
    ${ECHO_LIBMAD_DIR}/huffman.c
    ${ECHO_LIBMAD_DIR}/layer12.c
    ${ECHO_LIBMAD_DIR}/layer3.c
    ${ECHO_LIBMAD_DIR}/stream.c
    ${ECHO_LIBMAD_DIR}/synth.c
    ${ECHO_LIBMAD_DIR}/timer.c
    ${ECHO_LIBMAD_DIR}/version.c
)
target_include_directories(echo_libmad SYSTEM
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/libmad
    PRIVATE ${ECHO_LIBMAD_DIR}
)
target_compile_definitions(echo_libmad PRIVATE OPT_ACCURACY NDEBUG)
target_compile_options(echo_libmad PRIVATE -w)
if(CMAKE_CROSSCOMPILING)
    target_include_directories(echo_libmad SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/libmad_libc)
endif()

add_library(echo_device_logic STATIC
    audio_engine.c
    audio_math.c
    backlight.c
    battery_gauge.c
    bt_module.c
    core1_control.c
    core1_mp3.c
    core1_stream.c
    dac_sequence.c
    flac_source.c
    dma_loop.c
    emmc_layout.c
    exfat.c
    exfat_create.c
    fat.c
    fat_create.c
    fat_layout.c
    fat_names.c
    fat_reserve.c
    flac_accel.c
    image_crc.c
    jack_sense.c
    key_ladder.c
    key_scanner.c
    mp3_decoder.c
    mp3_probe.c
    msc.c
    panel.c
    panic_text.c
    pll.c
    start_record.c
    sd_card.c
    stall_watch.c
    tick_clock.c
    updater.c
    usb_device.c
    wav_source.c
)
target_include_directories(echo_device_logic PUBLIC include)
target_link_libraries(echo_device_logic
    PUBLIC echo_core echo_hal echo_codec echo_libmad
    PRIVATE echo_options
)

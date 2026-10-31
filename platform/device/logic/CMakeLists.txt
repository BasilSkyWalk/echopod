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

set(ECHO_FAAD2_DIR ${PROJECT_SOURCE_DIR}/third_party/faad2)
file(GLOB ECHO_FAAD2_SOURCES ${ECHO_FAAD2_DIR}/libfaad/*.c)
add_library(echo_faad2 STATIC ${ECHO_FAAD2_SOURCES})
target_include_directories(echo_faad2 SYSTEM
    PUBLIC ${ECHO_FAAD2_DIR}/include
    PRIVATE ${ECHO_FAAD2_DIR}/libfaad
)
target_compile_definitions(echo_faad2 PRIVATE
    FIXED_POINT LC_ONLY_DECODER DISABLE_SBR NDEBUG PACKAGE_VERSION="2.11.4"
)
target_compile_options(echo_faad2 PRIVATE -w)
set_target_properties(echo_faad2 PROPERTIES C_EXTENSIONS ON)
if(CMAKE_CROSSCOMPILING)
    target_compile_definitions(echo_faad2 PRIVATE abs=__builtin_abs)
    target_compile_options(echo_faad2 PRIVATE -O2)
    target_include_directories(echo_faad2 SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/libmad_libc)
endif()

set(ECHO_OPUS_DIR ${PROJECT_SOURCE_DIR}/third_party/opus)
file(GLOB ECHO_OPUS_SOURCES ${ECHO_OPUS_DIR}/celt/*.c ${ECHO_OPUS_DIR}/silk/*.c)
add_library(echo_opus STATIC
    ${ECHO_OPUS_SOURCES}
    ${ECHO_OPUS_DIR}/src/opus.c
    ${ECHO_OPUS_DIR}/src/opus_decoder.c
    ${ECHO_OPUS_DIR}/src/extensions.c
)
target_include_directories(echo_opus SYSTEM
    PUBLIC ${ECHO_OPUS_DIR}/include
    PRIVATE ${ECHO_OPUS_DIR} ${ECHO_OPUS_DIR}/celt ${ECHO_OPUS_DIR}/silk ${ECHO_OPUS_DIR}/silk/fixed
        ${ECHO_OPUS_DIR}/src
)
target_compile_definitions(echo_opus PRIVATE
    FIXED_POINT DISABLE_FLOAT_API OPUS_BUILD USE_ALLOCA NDEBUG PACKAGE_VERSION="1.6.1"
)
target_compile_options(echo_opus PRIVATE -w)
set_target_properties(echo_opus PROPERTIES C_EXTENSIONS ON)
if(CMAKE_CROSSCOMPILING)
    target_compile_definitions(echo_opus PRIVATE alloca=__builtin_alloca abs=__builtin_abs)
    target_compile_options(echo_opus PRIVATE -O2)
    target_include_directories(echo_opus SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/libmad_libc)
endif()

add_library(echo_device_logic STATIC
    audio_engine.c
    audio_math.c
    backlight.c
    battery_gauge.c
    watchdog.c
    bt_module.c
    card_write.c
    core1_aac.c
    core1_alac.c
    core1_control.c
    core1_mp3.c
    core1_opus.c
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
    stream_drift.c
    tick_clock.c
    updater.c
    usb_audio.c
    usb_audio_sink.c
    usb_device.c
    usb_fallback.c
    wav_source.c
)
if(CMAKE_CROSSCOMPILING)
    set_source_files_properties(
        bt_module.c
        core1_control.c
        dac_sequence.c
        emmc_layout.c
        exfat_create.c
        fat_create.c
        fat_reserve.c
        image_crc.c
        panic_text.c
        sd_card.c
        start_record.c
        updater.c
        usb_audio.c
        usb_device.c
        PROPERTIES COMPILE_OPTIONS -Os
    )
endif()
target_include_directories(echo_device_logic PUBLIC include)
target_link_libraries(echo_device_logic
    PUBLIC echo_core echo_hal echo_codec echo_libmad echo_faad2 echo_opus
    PRIVATE echo_options
)

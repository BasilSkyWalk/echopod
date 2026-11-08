add_executable(echo_device
    board/audio.c
    board/bluetooth.c
    board/clock.c
    board/clock_switch.c
    board/core1.c
    board/cru.c
    board/dac.c
    board/dmac.c
    board/display.c
    board/emmc.c
    board/glyphs.c
    board/gpio.c
    board/jack.c
    board/high_ram.c
    board/i2c.c
    board/language.c
    board/input.c
    board/libc.c
    board/loop_bench.c
    board/main.c
    board/mmc.c
    board/overlay.c
    board/panic.c
    board/power.c
    board/resident.c
    board/rtc.c
    board/sd.c
    board/stack.c
    board/start_info.c
    board/watchdog.c
    board/startup.c
    board/storage.c
    board/system.c
    board/usb.c
    board/usb_listen.c
    board/usb_report.c
)
set(ECHO_CORE1_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/core1/core1.ld)
add_executable(echo_core1
    core1/main.c
    core1/startup.c
    board/libc.c
    board/loop_bench.c
)
set_target_properties(echo_core1 PROPERTIES
    SUFFIX .elf
    LINK_DEPENDS ${ECHO_CORE1_LINKER_SCRIPT}
)
target_include_directories(echo_core1 PRIVATE board)
target_link_libraries(echo_core1 PRIVATE echo_codec_plain echo_device_logic_plain echo_options)
target_link_options(echo_core1 PRIVATE
    -fno-lto
    -T${ECHO_CORE1_LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/echo_core1.map
)
add_executable(echo_core1_mp3
    core1/mp3_main.c
    core1/startup.c
    core1/no_heap.c
    board/libc.c
)
set(ECHO_CORE1_MP3_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/core1/core1_mp3.ld)
set_target_properties(echo_core1_mp3 PROPERTIES
    SUFFIX .elf
    LINK_DEPENDS ${ECHO_CORE1_MP3_LINKER_SCRIPT}
)
target_include_directories(echo_core1_mp3 PRIVATE board)
target_link_libraries(echo_core1_mp3 PRIVATE echo_device_logic_plain echo_libmad echo_options gcc)
target_link_options(echo_core1_mp3 PRIVATE
    -fno-lto
    -T${ECHO_CORE1_MP3_LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/echo_core1_mp3.map
)
set(ECHO_CORE1_MP3_BINARY ${CMAKE_CURRENT_BINARY_DIR}/core1_mp3.bin)
add_custom_command(
    OUTPUT ${ECHO_CORE1_MP3_BINARY}
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:echo_core1_mp3> core1_mp3.bin
    DEPENDS echo_core1_mp3
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
)
add_executable(echo_core1_aac
    core1/aac_main.c
    core1/startup.c
    board/libc.c
)
set(ECHO_CORE1_AAC_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/core1/core1_aac.ld)
set_target_properties(echo_core1_aac PROPERTIES
    SUFFIX .elf
    LINK_DEPENDS ${ECHO_CORE1_AAC_LINKER_SCRIPT}
)
target_include_directories(echo_core1_aac PRIVATE board)
target_link_libraries(echo_core1_aac PRIVATE echo_device_logic_plain echo_faad2 echo_options gcc)
target_link_options(echo_core1_aac PRIVATE
    -fno-lto
    -T${ECHO_CORE1_AAC_LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/echo_core1_aac.map
)
set(ECHO_CORE1_AAC_BINARY ${CMAKE_CURRENT_BINARY_DIR}/core1_aac.bin)
add_custom_command(
    OUTPUT ${ECHO_CORE1_AAC_BINARY}
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:echo_core1_aac> core1_aac.bin
    DEPENDS echo_core1_aac
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
)
add_executable(echo_core1_opus
    core1/opus_main.c
    core1/startup.c
    core1/no_heap.c
    board/libc.c
)
set(ECHO_CORE1_OPUS_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/core1/core1_opus.ld)
set_target_properties(echo_core1_opus PROPERTIES
    SUFFIX .elf
    LINK_DEPENDS ${ECHO_CORE1_OPUS_LINKER_SCRIPT}
)
target_include_directories(echo_core1_opus PRIVATE board)
target_link_libraries(echo_core1_opus PRIVATE echo_device_logic_plain echo_opus echo_options gcc)
target_link_options(echo_core1_opus PRIVATE
    -fno-lto
    -T${ECHO_CORE1_OPUS_LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/echo_core1_opus.map
)
set(ECHO_CORE1_OPUS_BINARY ${CMAKE_CURRENT_BINARY_DIR}/core1_opus.bin)
add_custom_command(
    OUTPUT ${ECHO_CORE1_OPUS_BINARY}
    COMMAND ${CMAKE_OBJCOPY} -O binary -j .text $<TARGET_FILE:echo_core1_opus> core1_opus.bin
    DEPENDS echo_core1_opus
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
)
set(ECHO_CORE1_BINARY ${CMAKE_CURRENT_BINARY_DIR}/core1.bin)
add_custom_command(
    OUTPUT ${ECHO_CORE1_BINARY}
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:echo_core1> core1.bin
    DEPENDS echo_core1
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
)

set(ECHO_OVERLAY_IMPORTS
    app_dac_keys_start
    app_dac_keys_stop
    app_dac_next_key
    app_remember_dac_volume
    app_show_notice
    board_audio_stream_attenuation
    board_audio_stream_close
    board_audio_stream_consumed
    board_audio_stream_open
    board_audio_stream_pause
    board_audio_stream_play
    board_audio_stream_ring
    board_audio_volume_level
    board_display_service
    board_start_record_update
    board_wait_for_key
    covers_open
    emmc_read
    emmc_start
    emmc_status
    emmc_volume_read
    emmc_volume_write
    emmc_write
    fat_mount
    fat_open
    fat_read
    hal_audio_service
    hal_audio_set_volume
    hal_clock_milliseconds
    hal_display_set_power
    hal_input_next_transition
    hal_power_battery
    hash_update
    i18n_format
    i18n_text
    image_crc_update
    library_album_key
    library_count
    library_open
    library_read_album
    library_read_string
    library_read_track
    sd_health
    sd_prepare
    sd_read
    sd_sector_count
    sd_write
    spoken_kind_of
    storage_reset
    string_builder_append
    string_builder_append_char
    string_builder_append_unsigned
    string_builder_make
    update_record_save
    usb_connect
    usb_flush_fifos
    usb_power_down
    usb_report_append_hex
    usb_session_stop_listening
    usb_silence_audio
    usb_vbus_present
    utf8_next
    volume_is_internal_name
)
set(ECHO_DEVICE_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/board/echo_device.ld)
set_target_properties(echo_device PROPERTIES
    SUFFIX .elf
    LINK_DEPENDS ${ECHO_DEVICE_LINKER_SCRIPT}
)
set_source_files_properties(board/libc.c board/startup.c PROPERTIES
    COMPILE_OPTIONS "-fno-builtin;-fno-tree-loop-distribute-patterns;-fno-lto"
)
if(CMAKE_CROSSCOMPILING)
    set_source_files_properties(
        board/audio.c
        board/bluetooth.c
        board/clock_switch.c
        board/core1.c
        board/dac.c
        board/dmac.c
        board/emmc.c
        board/glyphs.c
        board/high_ram.c
        board/i2c.c
        board/input.c
        board/jack.c
        board/language.c
        board/main.c
        board/overlay.c
        board/panic.c
        board/power.c
        board/resident.c
        board/rtc.c
        board/sd.c
        board/stack.c
        board/start_info.c
        board/storage.c
        board/system.c
        board/update.c
        board/usb.c
        board/usb_dac_session.c
        board/usb_listen.c
        board/usb_report.c
        board/usb_session.c
        PROPERTIES COMPILE_OPTIONS -Os
    )
endif()
target_link_libraries(echo_device PRIVATE echo_app echo_device_logic echo_options echo_lto)
list(TRANSFORM ECHO_OVERLAY_IMPORTS PREPEND "-Wl,--require-defined=" OUTPUT_VARIABLE
    ECHO_OVERLAY_IMPORT_OPTIONS)
target_link_options(echo_device PRIVATE
    -flto -flto-partition=one
    ${ECHO_OVERLAY_IMPORT_OPTIONS}
    -T${ECHO_DEVICE_LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/echo_device.map
)

set(ECHO_OVERLAY_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/overlay/rescan_overlay.ld)
add_executable(echo_rescan_overlay overlay/rescan_overlay.c)
set_target_properties(echo_rescan_overlay PROPERTIES
    SUFFIX .elf
    LINK_DEPENDS "${ECHO_OVERLAY_LINKER_SCRIPT};$<TARGET_FILE:echo_device>"
)
add_dependencies(echo_rescan_overlay echo_device)
target_link_libraries(echo_rescan_overlay PRIVATE echo_library_scan echo_options gcc)
target_link_options(echo_rescan_overlay PRIVATE
    -fno-lto
    -T${ECHO_OVERLAY_LINKER_SCRIPT}
    -Wl,--just-symbols=$<TARGET_FILE:echo_device>
    -Wl,--allow-multiple-definition
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/echo_rescan_overlay.map
)

set(ECHO_USB_OVERLAY_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/overlay/usb_overlay.ld)
add_executable(echo_usb_overlay
    overlay/usb_overlay.c
    board/update.c
    board/usb_dac_session.c
    board/usb_port.c
    board/usb_session.c
)
set_target_properties(echo_usb_overlay PROPERTIES
    SUFFIX .elf
    LINK_DEPENDS "${ECHO_USB_OVERLAY_LINKER_SCRIPT};$<TARGET_FILE:echo_device>"
)
add_dependencies(echo_usb_overlay echo_device)
target_include_directories(echo_usb_overlay PRIVATE
    board
    $<TARGET_PROPERTY:echo_app,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:echo_settings,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:echo_playback,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:echo_library,INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_libraries(echo_usb_overlay PRIVATE echo_device_logic_plain echo_options gcc)
target_link_options(echo_usb_overlay PRIVATE
    -fno-lto
    -T${ECHO_USB_OVERLAY_LINKER_SCRIPT}
    -Wl,--just-symbols=$<TARGET_FILE:echo_device>
    -Wl,--allow-multiple-definition
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/echo_usb_overlay.map
)

set(ECHO_FIRMWARE_VERSION "0.11.1" CACHE STRING "major.minor.patch written to the image header")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(ECHO_DEVICE_IMAGE ${CMAKE_BINARY_DIR}/HIFIEC90.IMG)
set(ECHO_GLYPH_FACE_ARGUMENTS "")
foreach(face_file ${ECHO_GLYPH_FACE_FILES} ${ECHO_LANGUAGE_PACK_FILES})
    list(APPEND ECHO_GLYPH_FACE_ARGUMENTS --glyph-face ${face_file})
endforeach()

add_custom_target(echo_device_image ALL
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/make_image.py
        --version ${ECHO_FIRMWARE_VERSION}
        --elf $<TARGET_FILE:echo_device>
        --overlay $<TARGET_FILE:echo_rescan_overlay>
        --usb-overlay $<TARGET_FILE:echo_usb_overlay>
        --core1-mp3 ${ECHO_CORE1_MP3_BINARY}
        --core1-flac ${ECHO_CORE1_BINARY}
        --core1-aac ${ECHO_CORE1_AAC_BINARY}
        --core1-opus ${ECHO_CORE1_OPUS_BINARY}
        ${ECHO_GLYPH_FACE_ARGUMENTS}
        --output ${ECHO_DEVICE_IMAGE}
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/size_report.py
        --build ${CMAKE_BINARY_DIR} --summary --nm ${CMAKE_NM}
    BYPRODUCTS ${ECHO_DEVICE_IMAGE}
    DEPENDS echo_device echo_rescan_overlay echo_usb_overlay ${ECHO_CORE1_MP3_BINARY} ${ECHO_CORE1_BINARY}
        ${ECHO_CORE1_AAC_BINARY} ${ECHO_CORE1_OPUS_BINARY}
        ${PROJECT_SOURCE_DIR}/tools/make_image.py
    VERBATIM
)

add_custom_target(echo_size_report
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/size_report.py
        --build ${CMAKE_BINARY_DIR} --nm ${CMAKE_NM}
    DEPENDS echo_device echo_rescan_overlay echo_usb_overlay
    VERBATIM
    USES_TERMINAL
)

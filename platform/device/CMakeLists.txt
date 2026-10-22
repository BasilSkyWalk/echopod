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
    board/gpio.c
    board/jack.c
    board/high_ram.c
    board/input.c
    board/libc.c
    board/loop_bench.c
    board/main.c
    board/mmc.c
    board/overlay.c
    board/panic.c
    board/power.c
    board/sd.c
    board/start_info.c
    board/watchdog.c
    board/startup.c
    board/storage.c
    board/system.c
    board/update.c
    board/usb.c
    board/usb_report.c
    board/usb_session.c
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
target_link_libraries(echo_core1 PRIVATE echo_codec echo_device_logic echo_options)
target_link_options(echo_core1 PRIVATE
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
target_link_libraries(echo_core1_mp3 PRIVATE echo_device_logic echo_libmad echo_options gcc)
target_link_options(echo_core1_mp3 PRIVATE
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
set(ECHO_CORE1_BINARY ${CMAKE_CURRENT_BINARY_DIR}/core1.bin)
add_custom_command(
    OUTPUT ${ECHO_CORE1_BINARY}
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:echo_core1> core1.bin
    DEPENDS echo_core1
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
)

set(ECHO_DEVICE_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/board/echo_device.ld)
set_target_properties(echo_device PROPERTIES
    SUFFIX .elf
    LINK_DEPENDS ${ECHO_DEVICE_LINKER_SCRIPT}
)
set_source_files_properties(board/libc.c board/startup.c PROPERTIES
    COMPILE_OPTIONS "-fno-builtin;-fno-tree-loop-distribute-patterns"
)
target_link_libraries(echo_device PRIVATE echo_app echo_device_logic echo_options)
target_link_options(echo_device PRIVATE
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
    -T${ECHO_OVERLAY_LINKER_SCRIPT}
    -Wl,--just-symbols=$<TARGET_FILE:echo_device>
    -Wl,--allow-multiple-definition
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/echo_rescan_overlay.map
)

set(ECHO_IMAGE_HEADER "generated" CACHE STRING
    "Image outer header: reference (copied from RECHO 0.9.2) or generated (no reference needed)")
set_property(CACHE ECHO_IMAGE_HEADER PROPERTY STRINGS reference generated)
set(ECHO_FIRMWARE_VERSION "0.1.0" CACHE STRING "major.minor.patch written to a generated header")
set(ECHO_REFERENCE_IMG
    "$ENV{HOME}/echo-mini-blobs/recho-0.9.2/HIFIEC90.IMG"
    CACHE FILEPATH "RECHO 0.9.2 HIFIEC90.IMG; only its header bytes are copied"
)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(ECHO_DEVICE_IMAGE ${CMAKE_BINARY_DIR}/HIFIEC90.IMG)

if(ECHO_IMAGE_HEADER STREQUAL "generated")
    set(ECHO_IMAGE_HEADER_ARGUMENTS --version ${ECHO_FIRMWARE_VERSION})
elseif(ECHO_IMAGE_HEADER STREQUAL "reference")
    if(EXISTS ${ECHO_REFERENCE_IMG})
        set(ECHO_IMAGE_HEADER_ARGUMENTS --reference ${ECHO_REFERENCE_IMG})
    else()
        message(STATUS "reference image not found at ${ECHO_REFERENCE_IMG}: only the ELF is built")
    endif()
else()
    message(FATAL_ERROR "ECHO_IMAGE_HEADER must be reference or generated, not ${ECHO_IMAGE_HEADER}")
endif()

if(DEFINED ECHO_IMAGE_HEADER_ARGUMENTS)
    add_custom_target(echo_device_image ALL
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/make_image.py
            ${ECHO_IMAGE_HEADER_ARGUMENTS}
            --elf $<TARGET_FILE:echo_device>
            --overlay $<TARGET_FILE:echo_rescan_overlay>
            --core1-mp3 ${ECHO_CORE1_MP3_BINARY}
            --core1-flac ${ECHO_CORE1_BINARY}
            --output ${ECHO_DEVICE_IMAGE}
        BYPRODUCTS ${ECHO_DEVICE_IMAGE}
        DEPENDS echo_device echo_rescan_overlay ${ECHO_CORE1_MP3_BINARY} ${ECHO_CORE1_BINARY}
            ${PROJECT_SOURCE_DIR}/tools/make_image.py
        VERBATIM
    )
endif()

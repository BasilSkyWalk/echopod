add_executable(echo_device
    board/audio.c
    board/clock.c
    board/clock_switch.c
    board/cru.c
    board/display.c
    board/emmc.c
    board/gpio.c
    board/input.c
    board/libc.c
    board/main.c
    board/mmc.c
    board/power.c
    board/sd.c
    board/startup.c
    board/storage.c
    board/system.c
    board/update.c
    board/usb.c
    board/usb_report.c
    board/usb_session.c
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

set(ECHO_REFERENCE_IMG
    "$ENV{HOME}/echo-mini-blobs/recho-0.9.2/HIFIEC90.IMG"
    CACHE FILEPATH "RECHO 0.9.2 HIFIEC90.IMG; only its header bytes are copied"
)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(ECHO_DEVICE_IMAGE ${CMAKE_BINARY_DIR}/HIFIEC90.IMG)

if(EXISTS ${ECHO_REFERENCE_IMG})
    add_custom_target(echo_device_image ALL
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/make_image.py
            --reference ${ECHO_REFERENCE_IMG}
            --elf $<TARGET_FILE:echo_device>
            --output ${ECHO_DEVICE_IMAGE}
        BYPRODUCTS ${ECHO_DEVICE_IMAGE}
        DEPENDS echo_device ${PROJECT_SOURCE_DIR}/tools/make_image.py
        VERBATIM
    )
else()
    message(STATUS "reference image not found at ${ECHO_REFERENCE_IMG}: only the ELF is built")
endif()

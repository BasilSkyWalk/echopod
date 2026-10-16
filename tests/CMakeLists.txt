add_library(echo_check STATIC support/check.c)
target_include_directories(echo_check PUBLIC support)
target_link_libraries(echo_check PRIVATE echo_options)

function(echo_add_unit_test)
    cmake_parse_arguments(TEST "" "NAME" "SOURCES;LIBRARIES;PRIVATE_INCLUDES" ${ARGN})
    add_executable(${TEST_NAME} ${TEST_SOURCES})
    target_link_libraries(${TEST_NAME} PRIVATE echo_check echo_options ${TEST_LIBRARIES})
    target_include_directories(${TEST_NAME} PRIVATE ${TEST_PRIVATE_INCLUDES})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

echo_add_unit_test(NAME test_deadline SOURCES core/test_deadline.c LIBRARIES echo_core)
echo_add_unit_test(NAME test_frame_stats SOURCES core/test_frame_stats.c LIBRARIES echo_core)
echo_add_unit_test(NAME test_key_tracker SOURCES core/test_key_tracker.c LIBRARIES echo_core)
echo_add_unit_test(NAME test_utf8 SOURCES core/test_utf8.c LIBRARIES echo_core)
echo_add_unit_test(NAME test_string_builder SOURCES core/test_string_builder.c LIBRARIES echo_core)
echo_add_unit_test(NAME test_format SOURCES core/test_format.c LIBRARIES echo_core)
echo_add_unit_test(NAME test_random SOURCES core/test_random.c LIBRARIES echo_core)
echo_add_unit_test(NAME test_hash SOURCES core/test_hash.c LIBRARIES echo_core)
echo_add_unit_test(NAME test_power_policy SOURCES core/test_power_policy.c LIBRARIES echo_core)
echo_add_unit_test(NAME test_color SOURCES gfx/test_color.c LIBRARIES echo_gfx)
echo_add_unit_test(NAME test_rect SOURCES gfx/test_rect.c LIBRARIES echo_gfx)
echo_add_unit_test(NAME test_canvas SOURCES gfx/test_canvas.c LIBRARIES echo_gfx)
echo_add_unit_test(NAME test_image SOURCES gfx/test_image.c LIBRARIES echo_gfx)
echo_add_unit_test(NAME test_strip_renderer SOURCES gfx/test_strip_renderer.c LIBRARIES echo_gfx)
echo_add_unit_test(NAME test_frame_diff SOURCES gfx/test_frame_diff.c LIBRARIES echo_gfx)
echo_add_unit_test(NAME test_font SOURCES gfx/test_font.c gfx/tiny_font.c LIBRARIES echo_gfx)
echo_add_unit_test(NAME test_text SOURCES gfx/test_text.c gfx/tiny_font.c LIBRARIES echo_gfx)
echo_add_unit_test(NAME test_fonts SOURCES fonts/test_fonts.c LIBRARIES echo_fonts)

add_library(echo_library_fixtures STATIC library/memory_source.c library/fixture_library.c)
target_include_directories(echo_library_fixtures PUBLIC library)
target_compile_definitions(echo_library_fixtures PRIVATE
    ECHO_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
target_link_libraries(echo_library_fixtures PUBLIC echo_library PRIVATE echo_options)

foreach(playback_test play_queue resume_state)
    echo_add_unit_test(NAME test_${playback_test}
        SOURCES playback/test_${playback_test}.c
        LIBRARIES echo_playback echo_library_fixtures
    )
endforeach()

echo_add_unit_test(NAME test_settings SOURCES settings/test_settings.c LIBRARIES echo_settings)

foreach(library_test block_cache library library_browse favorites covers)
    echo_add_unit_test(NAME test_${library_test}
        SOURCES library/test_${library_test}.c
        LIBRARIES echo_library_fixtures
    )
endforeach()

echo_add_unit_test(NAME test_screen_stack SOURCES ui/test_screen_stack.c LIBRARIES echo_ui)
echo_add_unit_test(NAME test_navigation SOURCES ui/test_navigation.c LIBRARIES echo_ui)
echo_add_unit_test(NAME test_marquee SOURCES ui/test_marquee.c LIBRARIES echo_ui)
echo_add_unit_test(NAME test_fade SOURCES ui/test_fade.c LIBRARIES echo_ui)
echo_add_unit_test(NAME test_cover_flow SOURCES ui/test_cover_flow.c LIBRARIES echo_ui)
echo_add_unit_test(NAME test_label
    SOURCES ui/test_label.c gfx/tiny_font.c
    LIBRARIES echo_ui
    PRIVATE_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/gfx
)

add_library(echo_ui_fixtures STATIC gfx/tiny_font.c ui/ui_test_theme.c ui/ui_test_canvas.c)
target_include_directories(echo_ui_fixtures PUBLIC gfx ui)
target_link_libraries(echo_ui_fixtures PUBLIC echo_ui PRIVATE echo_options)

foreach(ui_test theme_layout list_view list_navigation header menu_screen message_screen
        choice_screen toast status_bar)
    echo_add_unit_test(NAME test_${ui_test}
        SOURCES ui/test_${ui_test}.c
        LIBRARIES echo_ui_fixtures
    )
endforeach()

echo_add_unit_test(NAME test_app
    SOURCES app/test_app.c app/fake_hal.c
    LIBRARIES echo_app echo_library_fixtures
    PRIVATE_INCLUDES ${PROJECT_SOURCE_DIR}/src/app
)
echo_add_unit_test(NAME test_player
    SOURCES app/test_player.c app/fake_hal.c
    LIBRARIES echo_app echo_library_fixtures
    PRIVATE_INCLUDES ${PROJECT_SOURCE_DIR}/src/app
)
echo_add_unit_test(NAME test_cover_cache
    SOURCES app/test_cover_cache.c app/fake_hal.c
    LIBRARIES echo_app echo_library_fixtures
    PRIVATE_INCLUDES ${PROJECT_SOURCE_DIR}/src/app
)
echo_add_unit_test(NAME test_stores
    SOURCES app/test_stores.c app/fake_hal.c
    LIBRARIES echo_app echo_library_fixtures
    PRIVATE_INCLUDES ${PROJECT_SOURCE_DIR}/src/app
)
echo_add_unit_test(NAME test_now_playing_screen
    SOURCES app/test_now_playing_screen.c app/fake_hal.c
    LIBRARIES echo_app
    PRIVATE_INCLUDES ${PROJECT_SOURCE_DIR}/src/app
)
echo_add_unit_test(NAME test_diagnostics_values
    SOURCES app/test_diagnostics_values.c app/fake_hal.c
    LIBRARIES echo_app
    PRIVATE_INCLUDES ${PROJECT_SOURCE_DIR}/src/app
)
echo_add_unit_test(NAME test_stopwatch_screen
    SOURCES app/test_stopwatch_screen.c app/fake_hal.c
    LIBRARIES echo_app
    PRIVATE_INCLUDES ${PROJECT_SOURCE_DIR}/src/app
)
echo_add_unit_test(NAME test_test_pattern_screen
    SOURCES app/test_test_pattern_screen.c app/fake_hal.c
    LIBRARIES echo_app
    PRIVATE_INCLUDES ${PROJECT_SOURCE_DIR}/src/app
)

foreach(device_test audio_math dac_sequence dma_loop key_ladder key_scanner msc panel pll sd_card tick_clock updater usb_device)
    echo_add_unit_test(NAME test_${device_test}
        SOURCES device/test_${device_test}.c
        LIBRARIES echo_device_logic
    )
endforeach()

echo_add_unit_test(NAME test_emmc_layout
    SOURCES device/test_emmc_layout.c device/fake_disk.c
    LIBRARIES echo_device_logic
)
echo_add_unit_test(NAME test_flac
    SOURCES codec/test_flac.c
    LIBRARIES echo_codec
)
target_compile_definitions(test_flac PRIVATE ECHO_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
echo_add_unit_test(NAME test_flac_bits
    SOURCES codec/test_flac_bits.c
    LIBRARIES echo_codec
)
target_include_directories(test_flac_bits PRIVATE ${PROJECT_SOURCE_DIR}/src/codec)
echo_add_unit_test(NAME test_audio_engine
    SOURCES device/test_audio_engine.c
    LIBRARIES echo_device_logic
)
target_compile_definitions(test_audio_engine PRIVATE ECHO_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
echo_add_unit_test(NAME test_fat
    SOURCES device/test_fat.c device/sparse_card.c
    LIBRARIES echo_device_logic
)
target_compile_definitions(test_fat PRIVATE ECHO_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
set_tests_properties(test_fat PROPERTIES TIMEOUT 20)

if(ECHO_BUILD_SIM)
    echo_add_unit_test(NAME test_sim_options
        SOURCES sim/test_sim_options.c
        LIBRARIES echo_sim_options
    )
    echo_add_unit_test(NAME test_sim_script
        SOURCES sim/test_sim_script.c
        LIBRARIES echo_sim_options
    )
    add_test(NAME sim_headless_smoke
        COMMAND echo_sim --headless --frames 30 --screenshot ${CMAKE_BINARY_DIR}/sim_smoke.bmp
    )
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(NAME no_comments
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/check_no_comments.py ${PROJECT_SOURCE_DIR}
)

find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    file(GLOB_RECURSE FORMATTED_SOURCES CONFIGURE_DEPENDS
        ${PROJECT_SOURCE_DIR}/src/*.c ${PROJECT_SOURCE_DIR}/src/*.h
        ${PROJECT_SOURCE_DIR}/platform/*.c ${PROJECT_SOURCE_DIR}/platform/*.h
        ${PROJECT_SOURCE_DIR}/tests/*.c ${PROJECT_SOURCE_DIR}/tests/*.h
    )
    add_test(NAME format COMMAND ${CLANG_FORMAT} --dry-run --Werror ${FORMATTED_SOURCES})
else()
    message(STATUS "clang-format not found: the format test is skipped")
endif()

find_program(ARM_GCC arm-none-eabi-gcc)
if(ARM_GCC)
    add_test(NAME target_audit
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/target_audit.py
            --source-root ${PROJECT_SOURCE_DIR}/src
            --budget-bytes ${ECHO_APP_RAM_BUDGET_BYTES}
    )
    add_test(NAME device_build
        COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
            -P ${PROJECT_SOURCE_DIR}/cmake/check_device_build.cmake
    )
    set_tests_properties(device_build PROPERTIES FIXTURES_SETUP device_image)
    add_executable(check_device_image device/check_device_image.c)
    target_link_libraries(check_device_image PRIVATE echo_device_logic echo_options)
    add_test(NAME device_image_passes_the_updater_check
        COMMAND check_device_image ${PROJECT_SOURCE_DIR}/build/device/HIFIEC90.IMG
    )
    set_tests_properties(device_image_passes_the_updater_check PROPERTIES
        FIXTURES_REQUIRED device_image
        SKIP_RETURN_CODE 77
    )
else()
    message(STATUS "arm-none-eabi-gcc not found: the target_audit test is skipped")
endif()

set(ECHO_TOOLS_PYTHON ${PROJECT_SOURCE_DIR}/.venv/bin/python)
if(EXISTS ${ECHO_TOOLS_PYTHON})
    add_test(NAME tools_unit_tests
        COMMAND ${ECHO_TOOLS_PYTHON} -m unittest discover -s ${PROJECT_SOURCE_DIR}/tools/tests
    )
    add_test(NAME fonts_up_to_date
        COMMAND ${ECHO_TOOLS_PYTHON} ${PROJECT_SOURCE_DIR}/tools/font_build.py
            --config ${PROJECT_SOURCE_DIR}/assets/fonts/fonts.json
            --output ${PROJECT_SOURCE_DIR}/src/fonts
            --style ${PROJECT_SOURCE_DIR}/.clang-format
            --check
    )
    add_test(NAME cover_fixture_up_to_date
        COMMAND ${ECHO_TOOLS_PYTHON} ${PROJECT_SOURCE_DIR}/tools/cover_fixture.py
            --output ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/small_covers.bin
            --check
    )
    add_test(NAME library_fixture_up_to_date
        COMMAND ${ECHO_TOOLS_PYTHON} ${PROJECT_SOURCE_DIR}/tools/library_fixture.py
            --output ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/small_library.db
            --check
    )
else()
    message(STATUS "tools venv not found: run python3 -m venv .venv && .venv/bin/pip install -r tools/requirements.txt")
endif()

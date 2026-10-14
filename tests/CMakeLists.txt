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
echo_add_unit_test(NAME test_color SOURCES gfx/test_color.c LIBRARIES echo_gfx)
echo_add_unit_test(NAME test_rect SOURCES gfx/test_rect.c LIBRARIES echo_gfx)
echo_add_unit_test(NAME test_canvas SOURCES gfx/test_canvas.c LIBRARIES echo_gfx)
echo_add_unit_test(NAME test_strip_renderer SOURCES gfx/test_strip_renderer.c LIBRARIES echo_gfx)
echo_add_unit_test(NAME test_app
    SOURCES app/test_app.c app/fake_hal.c
    LIBRARIES echo_app
    PRIVATE_INCLUDES ${PROJECT_SOURCE_DIR}/src/app
)

if(ECHO_BUILD_SIM)
    echo_add_unit_test(NAME test_sim_options
        SOURCES sim/test_sim_options.c
        LIBRARIES echo_sim_options
    )
    add_test(NAME sim_headless_smoke
        COMMAND echo_sim --headless --frames 30 --screenshot ${CMAKE_BINARY_DIR}/sim_smoke.bmp
    )
endif()

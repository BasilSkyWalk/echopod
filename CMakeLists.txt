cmake_minimum_required(VERSION 3.25)

project(echo_mini_fw LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ECHO_SANITIZE "Build host code with UndefinedBehaviorSanitizer" OFF)
option(ECHO_ADDRESS_SANITIZER "Build host code with AddressSanitizer" OFF)
option(ECHO_BUILD_SIM "Build the SDL2 simulator" ON)
option(ECHO_BUILD_TESTS "Build the host tests" ON)
option(ECHO_BUILD_DEVICE "Build the device firmware with cmake/arm-none-eabi.cmake" OFF)
set(ECHO_APP_RAM_BUDGET_BYTES 131072 CACHE STRING "Static RAM budget for src/ on the device")

include(cmake/project_options.cmake)

add_subdirectory(src)
add_subdirectory(platform/device/logic)

if(ECHO_BUILD_SIM)
    add_subdirectory(platform/sim)
endif()

if(ECHO_BUILD_DEVICE)
    add_subdirectory(platform/device)
endif()

if(ECHO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

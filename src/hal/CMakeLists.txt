add_library(echo_hal INTERFACE)
target_include_directories(echo_hal INTERFACE include)
target_link_libraries(echo_hal INTERFACE echo_core echo_dsp)

if(CMAKE_CROSSCOMPILING)
    add_library(echo_hal_plain INTERFACE)
    target_include_directories(echo_hal_plain INTERFACE include)
    target_link_libraries(echo_hal_plain INTERFACE echo_core_plain echo_dsp)
endif()

add_library(echo_hal INTERFACE)
target_include_directories(echo_hal INTERFACE include)
target_link_libraries(echo_hal INTERFACE echo_core echo_dsp)

set(ECHO_QOA_DIR ${PROJECT_SOURCE_DIR}/third_party/qoa)
add_library(echo_qoa STATIC ${ECHO_QOA_DIR}/qoa_impl.c)
target_include_directories(echo_qoa SYSTEM PUBLIC ${ECHO_QOA_DIR})
target_compile_options(echo_qoa PRIVATE -w -fwrapv)
if(CMAKE_CROSSCOMPILING)
    target_compile_options(echo_qoa PRIVATE -O2)
    target_include_directories(echo_qoa SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/libmad_libc)
endif()

add_library(echo_codec STATIC
    alac.c
    flac.c
    flac_bits.c
    mp4.c
    ogg.c
    qoa.c
)
if(CMAKE_CROSSCOMPILING)
    set_source_files_properties(mp4.c ogg.c PROPERTIES COMPILE_OPTIONS -Os)
endif()
target_include_directories(echo_codec PUBLIC include)
target_link_libraries(echo_codec
    PUBLIC echo_core
    PRIVATE echo_options echo_qoa
)

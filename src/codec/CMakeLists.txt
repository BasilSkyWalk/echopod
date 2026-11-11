set(ECHO_QOA_DIR ${PROJECT_SOURCE_DIR}/third_party/qoa)
add_library(echo_qoa STATIC ${ECHO_QOA_DIR}/qoa_impl.c)
target_include_directories(echo_qoa SYSTEM PUBLIC ${ECHO_QOA_DIR})
target_compile_options(echo_qoa PRIVATE -w -fwrapv)
if(CMAKE_CROSSCOMPILING)
    target_compile_options(echo_qoa PRIVATE -O2)
    target_include_directories(echo_qoa SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/libmad_libc)
endif()

set(ECHO_CODEC_SOURCES
    alac.c
    flac.c
    flac_bits.c
    mp4.c
    ogg.c
    qoa.c
)
add_library(echo_codec STATIC ${ECHO_CODEC_SOURCES})
if(CMAKE_CROSSCOMPILING)
    set_source_files_properties(mp4.c ogg.c PROPERTIES COMPILE_OPTIONS -Os)
    target_compile_definitions(echo_codec PRIVATE ECHO_RESIDENT_TABLES)
endif()
target_include_directories(echo_codec PUBLIC include)
target_link_libraries(echo_codec
    PUBLIC echo_core
    PRIVATE echo_options echo_lto echo_qoa
)

if(CMAKE_CROSSCOMPILING)
    add_library(echo_codec_plain STATIC ${ECHO_CODEC_SOURCES})
    target_include_directories(echo_codec_plain PUBLIC include)
    target_link_libraries(echo_codec_plain
        PUBLIC echo_core_plain
        PRIVATE echo_options echo_qoa
    )
endif()

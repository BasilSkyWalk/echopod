add_executable(echo_preview
    preview.c
    libc/libc.c
    ${PROJECT_SOURCE_DIR}/tests/app/fake_hal.c
)
set_target_properties(echo_preview PROPERTIES SUFFIX .wasm)
set_source_files_properties(libc/libc.c PROPERTIES COMPILE_OPTIONS -fno-builtin)
target_include_directories(echo_preview PRIVATE
    libc/include
    ${PROJECT_SOURCE_DIR}/tests/app
    ${PROJECT_SOURCE_DIR}/src/app
)
target_link_libraries(echo_preview PRIVATE echo_app echo_library_scan echo_options)
target_link_options(echo_preview PRIVATE -Wl,-z,stack-size=262144 -Wl,--export=__heap_base)

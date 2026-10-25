add_library(echo_options INTERFACE)

target_compile_options(echo_options INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wstrict-prototypes
    -Wmissing-prototypes
    -Wcast-qual
    -Wpointer-arith
    -Wundef
    -Wvla
    -Wdouble-promotion
    -Wimplicit-fallthrough
    -Wformat=2
)

add_library(echo_compact INTERFACE)
if(CMAKE_CROSSCOMPILING)
    target_compile_options(echo_compact INTERFACE -Os)
endif()

if(ECHO_SANITIZE)
    target_compile_options(echo_options INTERFACE
        -fsanitize=undefined
        -fno-sanitize-recover=all
        -fno-omit-frame-pointer
    )
    target_link_options(echo_options INTERFACE -fsanitize=undefined)
endif()

if(ECHO_ADDRESS_SANITIZER)
    target_compile_options(echo_options INTERFACE -fsanitize=address)
    target_link_options(echo_options INTERFACE -fsanitize=address)
endif()

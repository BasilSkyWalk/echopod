add_library(echo_core STATIC
    deadline.c
    format.c
    frame_stats.c
    hash.c
    i18n.c
    key_tracker.c
    low_battery.c
    permutation.c
    power_policy.c
    random.c
    string_builder.c
    utf8.c
    wall_clock.c
)
target_include_directories(echo_core PUBLIC include)
target_link_libraries(echo_core PRIVATE echo_options)
if(NOT CMAKE_CROSSCOMPILING)
    target_compile_definitions(echo_core PRIVATE ECHO_BUILTIN_ENGLISH)
endif()
set_source_files_properties(i18n.c string_builder.c format.c key_tracker.c low_battery.c permutation.c power_policy.c wall_clock.c PROPERTIES COMPILE_OPTIONS "$<$<BOOL:${CMAKE_CROSSCOMPILING}>:-Os>")
